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

#pragma once

#include "core/node.hpp"
#include "ngraph/node.hpp"

namespace ngraph
{
    namespace onnx_import
    {
        namespace op
        {
            namespace set_1
            {
                /// \brief      Creates nGraph node representing ONNX LpNormalization operator.
                ///
                ///             Suppose A contains spatial dimensions of input tensor, then
                ///             for matrix A we have p-norm defined as following double sum over
                ///             all elements:
                ///             ||A||_p = ||vec(A)||_p =
                ///                  [sum_{i=1}^m sum_{j=1}^n abs(a_{i,j})^p]^{1/p}
                ///
                /// \param[in]  node  The input ONNX node representing this operation.
                ///
                /// \return     Vector of nodes containting resulting nGraph nodes.
                ///
                OutputVector lp_norm(const Node& node);
            }
        }
    }
}
