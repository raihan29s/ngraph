/*******************************************************************************
* Copyright 2017-2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
#include <algorithm>
#include <map>

#include "ngraph/codegen/code_writer.hpp"
#include "ngraph/runtime/gpu/gpu_kernel_emitters.hpp"

using namespace ngraph;
using namespace ngraph::runtime::gpu::kernel;

void ngraph::runtime::gpu::kernel::emit_broadcast(codegen::CodeWriter& writer,
                                                  const std::string& element_type,
                                                  const std::string& arg0, // replacement context
                                                  const std::string& out,
                                                  const Shape& arg0_shape,
                                                  const Shape& out_shape,
                                                  const AxisSet& broadcast_axes)
{
}

//
// For the reference kernel this is gpud on, see ngraph/runtime/kernel/concat.hpp.
//
void ngraph::runtime::gpu::kernel::emit_concat(codegen::CodeWriter& writer,
                                               const std::string& element_type,
                                               const std::vector<std::string>& args,
                                               const std::string& out,
                                               const std::vector<Shape>& in_shapes,
                                               const Shape& out_shape,
                                               size_t concatenation_axis)
{
}

void ngraph::runtime::gpu::kernel::emit_replace_slice(
    codegen::CodeWriter& writer,
    const std::string& element_type,
    const std::string& arg0, // replacement context
    const std::string& arg1, // replacement value
    const std::string& out,
    const Shape& arg1_shape,
    const Shape& out_shape,
    const Coordinate& lower_bounds,
    const Coordinate& upper_bounds,
    const Strides& strides)
{
}

void ngraph::runtime::gpu::kernel::emit_slice(codegen::CodeWriter& writer,
                                              const std::string& element_type,
                                              const std::string& arg0, // replacement context
                                              const std::string& out,
                                              const Shape& arg0_shape,
                                              const Shape& out_shape,
                                              const Coordinate& lower_bounds,
                                              const Coordinate& upper_bounds,
                                              const Strides& strides)
{
}

void ngraph::runtime::gpu::kernel::emit_reshape(codegen::CodeWriter& writer,
                                                const std::string& element_type,
                                                const std::string& arg0, // replacement context
                                                const std::string& out,
                                                const Shape& arg0_shape,
                                                const Shape& out_shape,
                                                const AxisVector& arg0_axis_order)
{
}

void ngraph::runtime::gpu::kernel::emit_sum(codegen::CodeWriter& writer,
                                            const std::string& element_type,
                                            const std::string& arg0, // replacement context
                                            const std::string& out,
                                            const Shape& arg0_shape,
                                            const Shape& out_shape,
                                            const AxisSet& reduction_axes)
{
}
