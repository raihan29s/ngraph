//*****************************************************************************
// Copyright 2017-2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "ngraph/graph_util.hpp"
#include "ngraph/log.hpp"
#include "ngraph/op/broadcast.hpp"
#include "ngraph/op/concat.hpp"
#include "ngraph/op/constant.hpp"
#include "ngraph/op/convert.hpp"
#include "ngraph/op/experimental/shape_of.hpp"
#include "ngraph/op/fused/squeeze.hpp"
#include "ngraph/op/fused/unsqueeze.hpp"
#include "ngraph/op/non_zero.hpp"
#include "ngraph/op/pad.hpp"
#include "ngraph/op/reshape.hpp"
#include "ngraph/op/slice.hpp"
#include "ngraph/op/stop_gradient.hpp"
#include "ngraph/op/sum.hpp"
#include "ngraph/opsets/opset3.hpp"
#include "ngraph/util.hpp"
#include "nop_elimination.hpp"

using namespace std;
using namespace ngraph;

#define TI(x) x::type_info

static bool eliminate_nop(const std::shared_ptr<Node>& node)
{
    // skip if shapes are dynamic
    if (node->get_input_partial_shape(0).is_dynamic() ||
        node->get_output_partial_shape(0).is_dynamic())
    {
        return false;
    }

    if (node->get_input_shape(0) == node->get_output_shape(0))
    {
        return replace_output_update_name(node->output(0), node->input_value(0));
    }
    return false;
}

static bool eliminate_sum(const std::shared_ptr<Node>& node)
{
    auto sum = as_type_ptr<op::v0::Sum>(node);
    if (sum->get_reduction_axes().empty())
    {
        return replace_output_update_name(node->output(0), node->input_value(0));
    }
    return false;
}

static bool eliminate_convert(const std::shared_ptr<Node>& node)
{
    bool is_out_type_agnostic = false;
    static const std::set<NodeTypeInfo> type_agnostic{TI(opset3::NonZero)};
    if (node->output(0).get_target_inputs().size() == 1)
    {
        auto& out = *node->output(0).get_target_inputs().begin();
        is_out_type_agnostic = type_agnostic.count(out.get_node()->get_type_info()) == 1;
    }
    auto convert = as_type_ptr<opset3::Convert>(node);
    auto input = convert->input_value(0);
    if (convert->get_convert_element_type() == input.get_element_type() || is_out_type_agnostic)
    {
        if (is_out_type_agnostic && is_type<opset3::Convert>(input.get_node()))
        {
            input = input.get_node()->input_value(0);
        }
        return replace_output_update_name(node->output(0), input);
    }
    return false;
}

static bool eliminate_concat(const std::shared_ptr<Node>& node)
{
    auto node_input = node->input_value(0);

    // remove concat with single input
    if (node->get_input_size() == 1)
    {
        return replace_output_update_name(node->output(0), node_input);
    }
    return false;
}

static bool eliminate_reshape_v1(const std::shared_ptr<Node>& node)
{
    auto input = node->input_value(0);
    // check if reshape is not identity op
    if (input.get_partial_shape().is_dynamic() || node->get_output_partial_shape(0).is_dynamic())
    {
        NGRAPH_DEBUG << node << " has dynamic shapes.";
        return false;
    }
    // remove identity op
    if (input.get_shape() == node->get_output_shape(0))
    {
        return replace_output_update_name(node->output(0), input);
    }
    // eliminate redundant reshape, squeeze, or unsqueeze
    if (is_type<opset3::Squeeze>(input.get_node()) ||
        is_type<opset3::Unsqueeze>(input.get_node()) || is_type<opset3::Reshape>(input.get_node()))
    {
        auto shape = node->get_output_shape(0);
        std::vector<int64_t> vi;
        vi.assign(shape.begin(), shape.end());
        auto pat = op::Constant::create<int64_t>(element::i64, Shape{vi.size()}, vi);
        auto new_reshape =
            make_shared<opset3::Reshape>(input.get_node()->input_value(0), pat, false);
        return replace_output_update_name(node->output(0), new_reshape->output(0));
    }

    return false;
}

static std::shared_ptr<op::Constant> get_axes_remaining(const std::vector<uint64_t> from,
                                                        const std::vector<uint64_t> to,
                                                        bool is_rank_reducing = true)
{
    std::set<uint64_t> i1(from.begin(), from.end());
    std::set<uint64_t> i2(to.begin(), to.end());
    std::vector<int64_t> axes;
    std::set_difference(i1.begin(), i1.end(), i2.begin(), i2.end(), std::back_inserter(axes));
    if (axes.size() != 0)
    {
        return nullptr;
    }
    else
    {
        axes.clear();
        std::set_difference(i2.begin(), i2.end(), i1.begin(), i1.end(), std::back_inserter(axes));

        if (!is_rank_reducing)
        {
            for (size_t i = 0; i < axes.size(); i++)
            {
                if (axes[i] < i1.size())
                    continue;
                axes[i] -= i1.size();
            }
        }
        return op::Constant::create<int64_t>(element::i64, Shape{axes.size()}, axes);
    }
}

static bool is_equal_axes(const std::vector<uint64_t> from, const std::vector<uint64_t> to)
{
    std::set<uint64_t> i1(from.begin(), from.end());
    std::set<uint64_t> i2(to.begin(), to.end());
    std::vector<int64_t> axes;
    std::set_symmetric_difference(
        i1.begin(), i1.end(), i2.begin(), i2.end(), std::back_inserter(axes));
    return (axes.size() == 0);
}

static bool eliminate_unsqueeze(const std::shared_ptr<Node>& node)
{
    auto data_rank = node->input_value(0).get_partial_shape().rank();
    auto unsqueeze = as_type_ptr<opset3::Unsqueeze>(node);
    auto input = unsqueeze->input_value(0).get_node_shared_ptr();
    auto squeeze = as_type_ptr<opset3::Squeeze>(input);
    // eliminate redundant squeeze->unsqueeze
    if (squeeze && !data_rank.is_dynamic())
    {
        auto sq_axes = as_type_ptr<op::v0::Constant>(squeeze->input_value(1).get_node_shared_ptr());
        auto unsq_axes =
            as_type_ptr<op::v0::Constant>(unsqueeze->input_value(1).get_node_shared_ptr());
        if (!sq_axes || !unsq_axes)
        {
            NGRAPH_DEBUG << "squeeze->unsqueeze axes are not constants";
            return false;
        }

        auto sq_axes_val = squeeze->get_axes();
        auto unsq_axes_val = unsqueeze->get_axes();
        if (is_equal_axes(sq_axes_val, unsq_axes_val))
        {
            return replace_output_update_name(unsqueeze->output(0), squeeze->input_value(0));
        }

        auto sq_axes_const = get_axes_remaining(unsq_axes_val, sq_axes_val);
        if (sq_axes_const)
        {
            auto new_sq = make_shared<opset3::Squeeze>(
                squeeze->input_value(0).get_node_shared_ptr(), sq_axes_const);
            if (unsqueeze->get_output_partial_shape(0).same_scheme(
                    new_sq->get_output_partial_shape(0)))
            {
                return replace_output_update_name(unsqueeze->output(0), new_sq->output(0));
            }
        }
        auto unsq_axes_const = get_axes_remaining(sq_axes_val, unsq_axes_val);
        if (unsq_axes_const)
        {
            auto new_unsq = make_shared<opset3::Unsqueeze>(
                squeeze->input_value(0).get_node_shared_ptr(), unsq_axes_const);
            if (unsqueeze->get_output_partial_shape(0).same_scheme(
                    new_unsq->get_output_partial_shape(0)))
            {
                return replace_output_update_name(unsqueeze->output(0), new_unsq->output(0));
            }
        }
        return false;
    }

    // eliminate redundant unsqueeze
    if (as_type_ptr<opset3::Reshape>(input) && !node->get_output_partial_shape(0).is_dynamic())
    {
        auto shape = node->get_shape();
        std::vector<int64_t> vi;
        vi.assign(shape.begin(), shape.end());
        auto pat = op::Constant::create<int64_t>(element::i64, Shape{vi.size()}, vi);
        auto new_reshape =
            make_shared<opset3::Reshape>(input->input_value(0).get_node_shared_ptr(), pat, false);
        return replace_output_update_name(node->output(0), new_reshape->output(0));
    }
    return false;
}

static bool eliminate_squeeze(const std::shared_ptr<Node>& node)
{
    auto data_rank = node->input_value(0).get_partial_shape().rank();
    auto squeeze = as_type_ptr<opset3::Squeeze>(node);
    auto input = squeeze->input_value(0).get_node_shared_ptr();
    auto unsqueeze = as_type_ptr<opset3::Unsqueeze>(input);
    // eliminate redundant unsqueeze->squeeze
    if (unsqueeze && !data_rank.is_dynamic())
    {
        auto unsq_axes =
            as_type_ptr<op::v0::Constant>(unsqueeze->input_value(1).get_node_shared_ptr());
        auto sq_axes =
            as_type_ptr<op::v0::Constant>(unsqueeze->input_value(1).get_node_shared_ptr());
        if (!sq_axes || !unsq_axes)
        {
            NGRAPH_DEBUG << "unsqueeze->squeeze axes are not constants";
            return false;
        }

        auto unsq_axes_val = unsqueeze->get_axes();
        auto sq_axes_val = squeeze->get_axes();
        if (is_equal_axes(unsq_axes_val, sq_axes_val))
        {
            return replace_output_update_name(squeeze->output(0), unsqueeze->input_value(0));
        }

        auto sq_axes_const = get_axes_remaining(unsq_axes_val, sq_axes_val, false);
        if (sq_axes_const)
        {
            auto new_sq = make_shared<opset3::Squeeze>(
                unsqueeze->input_value(0).get_node_shared_ptr(), sq_axes_const);
            if (squeeze->get_output_partial_shape(0).same_scheme(
                    new_sq->get_output_partial_shape(0)))
            {
                return replace_output_update_name(squeeze->output(0), new_sq->output(0));
            }
        }
        auto unsq_axes_const = get_axes_remaining(sq_axes_val, unsq_axes_val, false);
        if (unsq_axes_const)
        {
            auto new_unsq = make_shared<opset3::Unsqueeze>(
                unsqueeze->input_value(0).get_node_shared_ptr(), unsq_axes_const);
            if (squeeze->get_output_partial_shape(0).same_scheme(
                    new_unsq->get_output_partial_shape(0)))
            {
                return replace_output_update_name(squeeze->output(0), new_unsq->output(0));
            }
        }
        return false;
    }

    // eliminate redundant squeeze
    if (as_type_ptr<opset3::Reshape>(input) && !node->get_output_partial_shape(0).is_dynamic())
    {
        auto shape = node->get_shape();
        std::vector<int64_t> vi;
        vi.assign(shape.begin(), shape.end());
        auto pat = op::Constant::create<int64_t>(element::i64, Shape{vi.size()}, vi);
        auto new_reshape =
            make_shared<opset3::Reshape>(input->input_value(0).get_node_shared_ptr(), pat, false);
        return replace_output_update_name(node->output(0), new_reshape->output(0));
    }
    return false;
}

static bool eliminate_stop_gradient(const std::shared_ptr<Node>& node)
{
    replace_output_update_name(node->output(0), node->input_value(0));
    return true;
}

static const std::unordered_map<NodeTypeInfo, std::function<bool(const std::shared_ptr<Node>&)>>
    dispatcher{{TI(op::v0::Pad), &eliminate_nop},
               {TI(opset3::Pad), &eliminate_nop},
               {TI(op::v0::Sum), &eliminate_sum},
               {TI(opset3::Convert), &eliminate_convert},
               {TI(op::v0::Slice), &eliminate_nop},
               {TI(op::v0::StopGradient), &eliminate_stop_gradient},
               {TI(opset3::Reshape), &eliminate_reshape_v1},
               {TI(opset3::Concat), &eliminate_concat},
               {TI(opset3::Squeeze), &eliminate_squeeze},
               {TI(opset3::Unsqueeze), &eliminate_unsqueeze},
               {TI(op::v0::Broadcast), &eliminate_nop}};

bool pass::NopElimination::run_on_function(std::shared_ptr<Function> function)
{
    bool clobbered = false;

    for (const auto& n : function->get_ops())
    {
        // Work around a warning [-Wpotentially-evaluated-expression]
        const Node& node = *n;
        auto handler = dispatcher.find(node.get_type_info());
        if (handler != dispatcher.end())
        {
            clobbered = handler->second(n) || clobbered;
        }
    }

    return clobbered;
}
