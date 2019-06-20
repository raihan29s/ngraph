//*****************************************************************************
// Copyright 2017-2019 Intel Corporation
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

#include "depth_to_space.hpp"
#include "ngraph/op/fused/depth_to_space.hpp"

namespace ngraph
{
    namespace onnx_import
    {
        namespace op
        {
            namespace set_1
            {
                NodeVector depth_to_space(const Node& node)
                {
                    auto data = node.get_ng_inputs().at(0);
                    std::size_t block_size = node.get_attribute_value<std::int64_t>("blocksize");
                    return NodeVector{std::make_shared<ngraph::op::DepthToSpace>(data, block_size)};
                }
            } // namespace set_1

        } //namespace op

    } // namespace onnx_import

} // namespace ngraph