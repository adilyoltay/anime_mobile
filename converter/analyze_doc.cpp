#include "core_builder.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/paint/fill.hpp"
#include <iostream>

int main()
{
    rive_converter::PropertyTypeMap map;
    rive_converter::CoreBuilder builder;

    auto& backboard = builder.addCore(new rive::Backboard());
    std::cout << "backboard.id=" << backboard.id << "\n";
    auto& artboard = builder.addCore(new rive::Artboard());
    std::cout << "artboard.id=" << artboard.id << "\n";
    builder.setParent(artboard, backboard.id);
    std::cout << " artboard parent stored="
              << std::get<uint32_t>(artboard.properties.back().value) << "\n";

    auto& shape = builder.addCore(new rive::Shape());
    std::cout << "shape.id=" << shape.id << "\n";
    builder.setParent(shape, artboard.id);
    std::cout << " shape parent stored="
              << std::get<uint32_t>(shape.properties.back().value) << "\n";

    auto& fill = builder.addCore(new rive::Fill());
    std::cout << "fill.id=" << fill.id << "\n";
    builder.setParent(fill, shape.id);
    std::cout << " fill parent stored="
              << std::get<uint32_t>(fill.properties.back().value) << "\n";
}
