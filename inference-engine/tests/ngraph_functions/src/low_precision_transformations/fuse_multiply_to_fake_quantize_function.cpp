// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ngraph_functions/low_precision_transformations/fuse_multiply_to_fake_quantize_function.hpp"

#include <ngraph/opsets/opset1.hpp>
#include "ngraph_ops/type_relaxed.hpp"
#include "ngraph_functions/subgraph_builders.hpp"
#include "transformations/low_precision/network_helper.hpp"

#include "ngraph_functions/low_precision_transformations/common/builders.hpp"
#include "ngraph_functions/low_precision_transformations/common/fake_quantize_on_data.hpp"
#include "ngraph_functions/low_precision_transformations/common/dequantization_operations.hpp"

namespace ngraph {
namespace builder {
namespace subgraph {

using namespace ngraph::pass;

std::shared_ptr<ngraph::Function> FuseMultiplyToFakeQuantizeFunction::get(
    const ngraph::Shape& inputShape,
    const FakeQuantizeOnData& fqOnData,
    const DequantizationOperations& dequantization) {
    const auto input = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, ngraph::Shape(inputShape));

    const auto fakeQuantize = makeFakeQuantize(input, ngraph::element::f32, fqOnData);
    const auto lastDequantization = makeDequantization(fakeQuantize, dequantization);
    lastDequantization->set_friendly_name("output");

    ngraph::ResultVector results{ std::make_shared<ngraph::opset1::Result>(lastDequantization) };
    return std::make_shared<ngraph::Function>(results, ngraph::ParameterVector{ input }, "FuseSubtractToFakeQuantizeFunction");
}

}  // namespace subgraph
}  // namespace builder
}  // namespace ngraph
