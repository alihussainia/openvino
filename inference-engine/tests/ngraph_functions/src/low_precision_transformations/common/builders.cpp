// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ngraph_functions/low_precision_transformations/common/builders.hpp"

#include <queue>
#include <memory>

#include <ngraph/opsets/opset1.hpp>
#include "ngraph_ops/type_relaxed.hpp"
#include "ngraph_functions/subgraph_builders.hpp"
#include "transformations/low_precision/network_helper.hpp"

namespace ngraph {
namespace builder {
namespace subgraph {

std::shared_ptr<Node> makeDequantization(
    const Output<Node>& data,
    const DequantizationOperations& dequantizationOperations) {
    Output<Node> parent = data;

    if (!dequantizationOperations.convert.empty()) {
        std::shared_ptr<ngraph::opset1::Convert> convert = std::make_shared<ngraph::pass::low_precision::DequantizationConvert>(
            data,
            dequantizationOperations.convert.outPrecision);
        parent = convert;
    }

    if (!dequantizationOperations.subtract.empty()) {
        std::shared_ptr<ngraph::opset1::Subtract> subtract;

        std::vector<size_t> shape;
        if (dequantizationOperations.subtract.constantShapeIsDefined) {
            shape = dequantizationOperations.subtract.constantShape;
        } else {
            if (dequantizationOperations.subtract.values.size() == 1ul) {
                shape = std::vector<size_t>({});
            } else {
                shape = std::vector<size_t>(parent.get_shape().size(), 1ul);
                shape[shape.size() >= 2 ? 1ul : 0] = dequantizationOperations.subtract.values.size();
            }
        }

        const auto subtractConst = std::make_shared<ngraph::opset1::Constant>(
            dequantizationOperations.subtract.constantPrecision != element::undefined ?
                dequantizationOperations.subtract.constantPrecision :
                parent.get_element_type(),
            shape,
            dequantizationOperations.subtract.values);

        if ((dequantizationOperations.subtract.outPrecision == element::undefined) ||
            (dequantizationOperations.subtract.outPrecision == parent.get_element_type())) {
            subtract = std::make_shared<ngraph::pass::low_precision::DequantizationSubtract>(parent, subtractConst);
        } else {
            subtract = std::make_shared<op::TypeRelaxed<ngraph::pass::low_precision::DequantizationSubtract>>(
                    std::vector<element::Type>{element::f32, element::f32},
                    std::vector<element::Type>{ element::f32 },
                    ngraph::op::TemporaryReplaceOutputType(parent, element::f32).get(),
                    ngraph::op::TemporaryReplaceOutputType(subtractConst, element::f32).get());
            ngraph::pass::low_precision::NetworkHelper::setOutDataPrecision(subtract, dequantizationOperations.subtract.outPrecision);
        }
        if (!dequantizationOperations.subtract.addDequantizationAttribute) {
            ngraph::pass::low_precision::NetworkHelper::cleanRunTimeInfo(subtract);
        }
        parent = subtract;
    }

    if (!dequantizationOperations.multiply.empty()) {
        std::vector<size_t> shape;
        if (dequantizationOperations.multiply.constantShapeIsDefined) {
            shape = dequantizationOperations.multiply.constantShape;
        } else {
            if (dequantizationOperations.multiply.values.size() == 1ul) {
                shape = std::vector<size_t>({});
            } else {
                shape = std::vector<size_t>(parent.get_shape().size(), 1ul);
                shape[shape.size() >= 2 ? 1ul : 0] = dequantizationOperations.multiply.values.size();
            }
        }

        std::shared_ptr<ngraph::opset1::Multiply> multiply;
        if ((dequantizationOperations.multiply.outPrecision == element::undefined) ||
            (dequantizationOperations.multiply.outPrecision == parent.get_element_type())) {
            const std::shared_ptr<ngraph::opset1::Constant> constant = std::make_shared<ngraph::opset1::Constant>(
                parent.get_element_type(),
                shape,
                dequantizationOperations.multiply.values);

            multiply = dequantizationOperations.multiply.constantIndex == 1ul ?
                std::make_shared<ngraph::pass::low_precision::DequantizationMultiply>(parent, constant) :
                std::make_shared<ngraph::pass::low_precision::DequantizationMultiply>(constant, parent);
        } else {
            const std::shared_ptr<ngraph::opset1::Constant> constant = std::make_shared<ngraph::opset1::Constant>(
                dequantizationOperations.multiply.constantPrecision != element::undefined ?
                    dequantizationOperations.multiply.constantPrecision :
                    parent.get_element_type(),
                shape,
                dequantizationOperations.multiply.values);

            multiply = dequantizationOperations.multiply.constantIndex == 1ul ?
                std::make_shared<op::TypeRelaxed<ngraph::pass::low_precision::DequantizationMultiply>>(
                    std::vector<element::Type>{element::f32, element::f32},
                    std::vector<element::Type>{ element::f32 },
                    ngraph::op::TemporaryReplaceOutputType(parent, element::f32).get(),
                    ngraph::op::TemporaryReplaceOutputType(constant, element::f32).get()) :
                std::make_shared<op::TypeRelaxed<ngraph::pass::low_precision::DequantizationMultiply>>(
                    std::vector<element::Type>{element::f32, element::f32},
                    std::vector<element::Type>{ element::f32 },
                    ngraph::op::TemporaryReplaceOutputType(constant, element::f32).get(),
                    ngraph::op::TemporaryReplaceOutputType(parent, element::f32).get());
        }

        parent = multiply;
    }

    return parent.get_node_shared_ptr();
}

std::shared_ptr<ngraph::opset1::FakeQuantize> makeFakeQuantize(
    const Output<Node>& input,
    const ngraph::element::Type precision,
    const FakeQuantizeOnData& fqOnData) {
    return as_type_ptr<ngraph::opset1::FakeQuantize>(ngraph::builder::makeFakeQuantize(
        input,
        precision,
        fqOnData.quantizationLevel,
        fqOnData.constantShape,
        fqOnData.inputLowValues,
        fqOnData.inputHighValues,
        fqOnData.outputLowValues,
        fqOnData.outputHighValues));
}

std::shared_ptr<ngraph::opset1::FakeQuantize> makeFakeQuantizeTypeRelaxed(
    const std::shared_ptr<ngraph::Node>& input,
    const ngraph::element::Type precision,
    const FakeQuantizeOnData& fqOnData) {
    const std::shared_ptr<ngraph::opset1::FakeQuantize> fq = makeFakeQuantize(input, precision, fqOnData);
    return std::make_shared<ngraph::op::TypeRelaxed<ngraph::opset1::FakeQuantize>>(*fq, fqOnData.outputPrecision);
}

} // namespace subgraph
} // namespace builder
} // namespace ngraph
