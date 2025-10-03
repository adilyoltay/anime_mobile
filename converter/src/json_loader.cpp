#include "json_loader.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>

namespace
{
uint32_t parse_color_string(const std::string& color_string,
                            uint32_t fallback)
{
    if (color_string.empty())
    {
        return fallback;
    }

    std::string normalized = color_string;
    if (normalized[0] == '#')
    {
        normalized = normalized.substr(1);
    }
    else if (normalized.rfind("0x", 0) == 0 || normalized.rfind("0X", 0) == 0)
    {
        normalized = normalized.substr(2);
    }

    try
    {
        uint32_t value = std::stoul(normalized, nullptr, 16);
        if (normalized.length() <= 6)
        {
            value |= 0xFF000000u;
        }
        return value;
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

rive_converter::ShapeType parse_shape_type(const std::string& type)
{
    std::string lowered = type;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "ellipse" || lowered == "circle")
    {
        return rive_converter::ShapeType::ellipse;
    }
    else if (lowered == "triangle")
    {
        return rive_converter::ShapeType::triangle;
    }
    else if (lowered == "polygon")
    {
        return rive_converter::ShapeType::polygon;
    }
    else if (lowered == "star")
    {
        return rive_converter::ShapeType::star;
    }
    else if (lowered == "image")
    {
        return rive_converter::ShapeType::image;
    }
    else if (lowered == "clipping" || lowered == "clip")
    {
        return rive_converter::ShapeType::clipping;
    }
    else if (lowered == "path")
    {
        return rive_converter::ShapeType::path;
    }
    return rive_converter::ShapeType::rectangle;
}

std::vector<uint16_t> parse_uint16_list(const nlohmann::json& values)
{
    std::vector<uint16_t> out;
    if (!values.is_array())
    {
        return out;
    }
    for (const auto& entry : values)
    {
        if (entry.is_number_integer() || entry.is_number_unsigned())
        {
            int64_t v = entry.get<int64_t>();
            if (v < 0)
            {
                continue;
            }
            if (v > 0xFFFF)
            {
                v = 0xFFFF;
            }
            out.push_back(static_cast<uint16_t>(v));
        }
    }
    return out;
}

std::vector<std::string> parse_string_list(const nlohmann::json& values)
{
    std::vector<std::string> out;
    if (!values.is_array())
    {
        return out;
    }
    for (const auto& entry : values)
    {
        if (entry.is_string())
        {
            out.push_back(entry.get<std::string>());
        }
    }
    return out;
}

rive_converter::DataBindData parse_data_bind_json(const nlohmann::json& json)
{
    rive_converter::DataBindData bind;

    if (json.contains("typeKey"))
    {
        bind.typeKey = static_cast<uint16_t>(json.value("typeKey", static_cast<int>(bind.typeKey)));
    }

    if (json.contains("localId"))
    {
        const auto& value = json["localId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                bind.localId = static_cast<uint32_t>(v);
            }
        }
    }

    auto parse_target_local_id = [&](const char* key) {
        if (json.contains(key))
        {
            const auto& value = json[key];
            if (value.is_number_integer() || value.is_number_unsigned())
            {
                int64_t v = value.get<int64_t>();
                if (v >= 0)
                {
                    bind.targetLocalId = static_cast<uint32_t>(v);
                }
            }
        }
    };

    parse_target_local_id("targetLocalId");
    parse_target_local_id("targetId");

    if (json.contains("targetName") && json["targetName"].is_string())
    {
        bind.targetName = json["targetName"].get<std::string>();
    }

    if (json.contains("targetPathNames"))
    {
        bind.targetPathNames = parse_string_list(json["targetPathNames"]);
    }
    else if (json.contains("targetPath"))
    {
        bind.targetPathNames = parse_string_list(json["targetPath"]);
    }

    bind.propertyKey = json.value("propertyKey", bind.propertyKey);
    bind.flags = json.value("flags", bind.flags);

    if (json.contains("converterId"))
    {
        const auto& value = json["converterId"];
        if (value.is_null())
        {
            bind.converterId = -1;
        }
        else if (value.is_number_integer() || value.is_number_unsigned())
        {
            bind.converterId = static_cast<int32_t>(value.get<int64_t>());
        }
    }

    if (json.contains("converterLocalId"))
    {
        const auto& value = json["converterLocalId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                bind.converterLocalId = static_cast<uint32_t>(v);
            }
        }
    }

    if (json.contains("sourcePathIds"))
    {
        bind.sourcePathIds = parse_uint16_list(json["sourcePathIds"]);
    }

    bind.toSource = json.value("toSource", bind.toSource);

    return bind;
}

rive_converter::DataConverterData parse_data_converter_json(const nlohmann::json& json)
{
    rive_converter::DataConverterData converter;

    converter.typeKey = static_cast<uint16_t>(json.value("typeKey", static_cast<int>(converter.typeKey)));

    if (json.contains("localId"))
    {
        const auto& value = json["localId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                converter.localId = static_cast<uint32_t>(v);
            }
        }
    }

    converter.name = json.value("name", converter.name);
    converter.interpolationType = json.value("interpolationType", converter.interpolationType);

    if (json.contains("interpolatorId"))
    {
        const auto& value = json["interpolatorId"];
        if (value.is_null())
        {
            converter.interpolatorId = -1;
        }
        else if (value.is_number_integer() || value.is_number_unsigned())
        {
            converter.interpolatorId = static_cast<int32_t>(value.get<int64_t>());
        }
    }

    converter.flags = json.value("flags", converter.flags);
    converter.minInput = json.value("minInput", converter.minInput);
    converter.maxInput = json.value("maxInput", converter.maxInput);
    converter.minOutput = json.value("minOutput", converter.minOutput);
    converter.maxOutput = json.value("maxOutput", converter.maxOutput);
    converter.decimals = json.value("decimals", converter.decimals);
    converter.colorFormat = json.value("colorFormat", converter.colorFormat);

    if (json.contains("contexts") && json["contexts"].is_array())
    {
        for (const auto& ctx : json["contexts"])
        {
            converter.contexts.push_back(parse_data_bind_json(ctx));
        }
    }

    return converter;
}

rive_converter::StateMachineListenerActionData parse_listener_action_json(
    const nlohmann::json& json)
{
    rive_converter::StateMachineListenerActionData action;

    action.typeKey = static_cast<uint16_t>(json.value("typeKey", static_cast<int>(action.typeKey)));

    if (json.contains("localId"))
    {
        const auto& value = json["localId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                action.localId = static_cast<uint32_t>(v);
            }
        }
    }

    if (json.contains("inputId"))
    {
        const auto& value = json["inputId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                action.inputLocalId = static_cast<uint32_t>(v);
            }
        }
    }

    if (json.contains("input") && json["input"].is_string())
    {
        action.inputName = json["input"].get<std::string>();
    }

    if (json.contains("nestedInputId"))
    {
        const auto& value = json["nestedInputId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                action.nestedInputLocalId = static_cast<uint32_t>(v);
            }
        }
    }

    if (json.contains("nestedInput") && json["nestedInput"].is_string())
    {
        action.nestedInputName = json["nestedInput"].get<std::string>();
    }

    action.value = json.value("value", action.value);

    if (json.contains("dataBinds") && json["dataBinds"].is_array())
    {
        for (const auto& bindJson : json["dataBinds"])
        {
            action.dataBinds.push_back(parse_data_bind_json(bindJson));
        }
    }

    return action;
}

rive_converter::StateMachineListenerData parse_listener_json(const nlohmann::json& json)
{
    rive_converter::StateMachineListenerData listener;

    listener.typeKey = static_cast<uint16_t>(json.value("typeKey", static_cast<int>(listener.typeKey)));

    if (json.contains("localId"))
    {
        const auto& value = json["localId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                listener.localId = static_cast<uint32_t>(v);
            }
        }
    }

    listener.name = json.value("name", listener.name);

    if (json.contains("targetId"))
    {
        const auto& value = json["targetId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                listener.targetLocalId = static_cast<uint32_t>(v);
            }
        }
    }

    if (json.contains("targetLocalId"))
    {
        const auto& value = json["targetLocalId"];
        if (value.is_number_integer() || value.is_number_unsigned())
        {
            int64_t v = value.get<int64_t>();
            if (v >= 0)
            {
                listener.targetLocalId = static_cast<uint32_t>(v);
            }
        }
    }

    if (json.contains("targetName") && json["targetName"].is_string())
    {
        listener.targetName = json["targetName"].get<std::string>();
    }

    listener.listenerTypeValue = json.value("listenerTypeValue", listener.listenerTypeValue);

    if (json.contains("eventId"))
    {
        const auto& value = json["eventId"];
        if (value.is_null())
        {
            listener.eventLocalId = -1;
        }
        else if (value.is_number_integer() || value.is_number_unsigned())
        {
            listener.eventLocalId = static_cast<int32_t>(value.get<int64_t>());
        }
    }

    if (json.contains("eventName") && json["eventName"].is_string())
    {
        listener.eventName = json["eventName"].get<std::string>();
    }

    if (json.contains("viewModelPathIds"))
    {
        listener.viewModelPathIds = parse_uint16_list(json["viewModelPathIds"]);
    }

    if (json.contains("viewModelPathNames"))
    {
        listener.viewModelPathNames = parse_string_list(json["viewModelPathNames"]);
    }

    if (json.contains("actions") && json["actions"].is_array())
    {
        for (const auto& actionJson : json["actions"])
        {
            listener.actions.push_back(parse_listener_action_json(actionJson));
        }
    }

    if (json.contains("dataBinds") && json["dataBinds"].is_array())
    {
        for (const auto& bindJson : json["dataBinds"])
        {
            listener.dataBinds.push_back(parse_data_bind_json(bindJson));
        }
    }

    return listener;
}
} // namespace

namespace rive_converter
{
Document parse_json(const std::string& json_content)
{
    Document doc;
    auto json = nlohmann::json::parse(json_content);

    // Check if we have new "artboards" array format
    bool hasArtboardsArray = json.contains("artboards") && json["artboards"].is_array();
    
    // If artboards array exists, parse each one
    if (hasArtboardsArray)
    {
        for (const auto& artboardJson : json["artboards"])
        {
            ArtboardData artboardData;
            artboardData.name = artboardJson.value("name", std::string("Artboard"));
            artboardData.width = artboardJson.value("width", 400.0f);
            artboardData.height = artboardJson.value("height", 300.0f);
            
            // Parse shapes for this artboard - FULL schema (matching legacy parse_shape_array)
            if (artboardJson.contains("shapes"))
            {
                for (const auto& shape : artboardJson["shapes"])
                {
                    ShapeData data;
                    data.type = parse_shape_type(shape.value("type", "rectangle"));
                    data.x = shape.value("x", data.x);
                    data.y = shape.value("y", data.y);
                    data.width = shape.value("width", data.width);
                    data.height = shape.value("height", data.height);
                    data.points = shape.value("points", data.points);
                    data.cornerRadius = shape.value("cornerRadius", data.cornerRadius);
                    data.innerRadius = shape.value("innerRadius", data.innerRadius);
                    data.assetId = shape.value("assetId", data.assetId);
                    data.originX = shape.value("originX", data.originX);
                    data.originY = shape.value("originY", data.originY);
                    data.sourceId = shape.value("sourceId", data.sourceId);
                    data.fillRule = shape.value("fillRule", data.fillRule);
                    data.clipVisible = shape.value("clipVisible", data.clipVisible);

                    // Parse gradient
                    if (shape.contains("gradient"))
                    {
                        const auto& grad = shape["gradient"];
                        data.fill.hasGradient = true;
                        data.fill.enabled = true;
                        data.fill.gradient.type = grad.value("type", std::string("radial"));
                        
                        if (grad.contains("stops"))
                        {
                            for (const auto& stop : grad["stops"])
                            {
                                GradientStop gs;
                                gs.position = stop.value("position", 0.0f);
                                std::string color = stop.value("color", std::string("#FFFFFF"));
                                gs.color = parse_color_string(color, 0xFFFFFFFF);
                                data.fill.gradient.stops.push_back(gs);
                    }
                    else if (shape.contains("fill"))
                    {
                        const auto& fill = shape["fill"];
                        std::string color = fill.value("color", std::string());
                        if (!color.empty())
                        {
                            data.fill.color = parse_color_string(color, data.fill.color);
                            data.fill.enabled = true;
                        }
                    }
                    else if (shape.contains("color"))
                    {
                        std::string color = shape["color"].get<std::string>();
                        data.fill.color = parse_color_string(color, data.fill.color);
                        data.fill.enabled = true;
                    }

                    // Parse stroke
                    if (shape.contains("stroke"))
                    {
                        const auto& stroke = shape["stroke"];
                        data.stroke.enabled = true;
                        data.stroke.thickness = stroke.value("thickness", data.stroke.thickness);
                        std::string color = stroke.value("color", std::string());
                        if (!color.empty())
                        {
                            data.stroke.color = parse_color_string(color, data.stroke.color);
                        }
                        
                        // Parse dash
                        if (stroke.contains("dash"))
                        {
                            const auto& dash = stroke["dash"];
                            data.stroke.dash.enabled = true;
                            data.stroke.dash.length = dash.value("length", data.stroke.dash.length);
                            data.stroke.dash.gap = dash.value("gap", data.stroke.dash.gap);
                            data.stroke.dash.lengthIsPercentage = dash.value("isPercentage", false);
                        }
                        
                        // Parse trimPath
                        if (stroke.contains("trimPath"))
                        {
                            const auto& trim = stroke["trimPath"];
                            data.stroke.trimPath.enabled = true;
                            data.stroke.trimPath.start = trim.value("start", data.stroke.trimPath.start);
                            data.stroke.trimPath.end = trim.value("end", data.stroke.trimPath.end);
                            data.stroke.trimPath.offset = trim.value("offset", data.stroke.trimPath.offset);
                            data.stroke.trimPath.mode = trim.value("mode", data.stroke.trimPath.mode);
                        }
                    }
                    
                    // Parse feather
                    if (shape.contains("feather"))
                    {
                        const auto& feather = shape["feather"];
                        data.fill.feather.enabled = true;
                        data.fill.feather.strength = feather.value("strength", data.fill.feather.strength);
                        data.fill.feather.offsetX = feather.value("offsetX", data.fill.feather.offsetX);
                        data.fill.feather.offsetY = feather.value("offsetY", data.fill.feather.offsetY);
                        data.fill.feather.inner = feather.value("inner", data.fill.feather.inner);
                    }
                    
                    artboardData.shapes.push_back(data);
                }
            }
            
            // Parse animations for this artboard
            if (artboardJson.contains("animations"))
            {
                for (const auto& anim : artboardJson["animations"])
                {
                    AnimationData animData;
                    animData.name = anim.value("name", animData.name);
                    animData.fps = anim.value("fps", animData.fps);
                    animData.duration = anim.value("duration", animData.duration);
                    animData.loop = anim.value("loop", animData.loop);
                    artboardData.animations.push_back(animData);
                }
            }
            
            // Parse state machines for this artboard - FULL (inputs, layers, states, transitions)
            if (artboardJson.contains("stateMachines"))
            {
                for (const auto& sm : artboardJson["stateMachines"])
                {
                    StateMachineData smData;
                    smData.name = sm.value("name", smData.name);
                    
                    // Parse inputs (CRITICAL - drives transitions!)
                    if (sm.contains("inputs"))
                    {
                        for (const auto& input : sm["inputs"])
                        {
                            StateMachineInputData inputData;
                            inputData.name = input.value("name", inputData.name);
                            inputData.type = input.value("type", inputData.type);
                            inputData.defaultValue = input.value("value", input.value("defaultValue", inputData.defaultValue));
                            smData.inputs.push_back(inputData);
                        }
                    }
                    
                    // Parse layers
                    if (sm.contains("layers"))
                    {
                        for (const auto& layer : sm["layers"])
                        {
                            LayerData layerData;
                            layerData.name = layer.value("name", layerData.name);
                            
                            // Parse states
                            if (layer.contains("states"))
                            {
                                for (const auto& state : layer["states"])
                                {
                                    StateData stateData;
                                    stateData.name = state.value("name", stateData.name);
                                    stateData.type = state.value("type", stateData.type);
                                    stateData.animationName = state.value("animationName", stateData.animationName);
                                    layerData.states.push_back(stateData);
                                }
                            }
                            
                            // Parse transitions (CRITICAL - allows state changes!)
                            if (layer.contains("transitions"))
                            {
                                for (const auto& trans : layer["transitions"])
                                {
                                    TransitionData transData;
                                    transData.from = trans.value("from", transData.from);
                                    transData.to = trans.value("to", transData.to);
                                    transData.duration = trans.value("duration", transData.duration);
                                    
                                    // Parse conditions
                                    if (trans.contains("conditions"))
                                    {
                                        for (const auto& cond : trans["conditions"])
                                        {
                                            TransitionConditionData condData;
                                            condData.input = cond.value("input", cond.value("inputName", condData.input));
                                            condData.op = cond.value("op", condData.op);
                                            condData.value = cond.value("value", condData.value);
                                            transData.conditions.push_back(condData);
                                        }
                                    }
                                    
                                    layerData.transitions.push_back(transData);
                                }
                    }
                    
                    smData.layers.push_back(layerData);
                }
            }

            if (sm.contains("listeners") && sm["listeners"].is_array())
            {
                for (const auto& listenerJson : sm["listeners"])
                {
                    smData.listeners.push_back(parse_listener_json(listenerJson));
                }
            }

            if (sm.contains("dataBinds") && sm["dataBinds"].is_array())
            {
                for (const auto& bindJson : sm["dataBinds"])
                {
                    smData.dataBinds.push_back(parse_data_bind_json(bindJson));
                }
            }

            if (sm.contains("dataBindings") && sm["dataBindings"].is_array())
            {
                for (const auto& bindJson : sm["dataBindings"])
                {
                    smData.dataBinds.push_back(parse_data_bind_json(bindJson));
                }
            }
                    
                    artboardData.stateMachines.push_back(smData);
                }
            }

            if (artboardJson.contains("dataConverters") &&
                artboardJson["dataConverters"].is_array())
            {
                for (const auto& converterJson : artboardJson["dataConverters"])
                {
                    artboardData.dataConverters.push_back(
                        parse_data_converter_json(converterJson));
                }
            }

            auto append_data_binds = [&](const nlohmann::json& array) {
                for (const auto& bindJson : array)
                {
                    artboardData.dataBinds.push_back(parse_data_bind_json(bindJson));
                }
            };

            if (artboardJson.contains("dataBinds") &&
                artboardJson["dataBinds"].is_array())
            {
                append_data_binds(artboardJson["dataBinds"]);
            }

            if (artboardJson.contains("dataBindings") &&
                artboardJson["dataBindings"].is_array())
            {
                append_data_binds(artboardJson["dataBindings"]);
            }
            
            // Parse additional content that may be inside artboard in new format
            // TODO: Add parsing for customPaths, texts, constraints, events, bones, skins
            // For now these will be parsed at root level and added to first artboard via legacy migration
            
            doc.artboards.push_back(std::move(artboardData));
        }
        
        // DON'T return early - continue to parse root-level content
        // Root-level content will be added to first artboard via legacy migration below
    }

    // Artboard info will be handled in legacy support section at end

    auto parse_shape_array = [&](const nlohmann::json& shapes) {
        for (const auto& shape : shapes)
        {
            ShapeData data;
            data.type = parse_shape_type(shape.value("type", "rectangle"));
            data.x = shape.value("x", data.x);
            data.y = shape.value("y", data.y);
            data.width = shape.value("width", data.width);
            data.height = shape.value("height", data.height);
            data.points = shape.value("points", data.points);
            data.cornerRadius = shape.value("cornerRadius", data.cornerRadius);
            data.innerRadius = shape.value("innerRadius", data.innerRadius);
            data.assetId = shape.value("assetId", data.assetId);
            data.originX = shape.value("originX", data.originX);
            data.originY = shape.value("originY", data.originY);
            data.sourceId = shape.value("sourceId", data.sourceId);
            data.fillRule = shape.value("fillRule", data.fillRule);
            data.clipVisible = shape.value("clipVisible", data.clipVisible);

            // Parse gradient
            if (shape.contains("gradient"))
            {
                const auto& grad = shape["gradient"];
                data.fill.hasGradient = true;
                data.fill.enabled = true;
                data.fill.gradient.type = grad.value("type", std::string("radial"));
                
                if (grad.contains("stops"))
                {
                    for (const auto& stop : grad["stops"])
                    {
                        GradientStop gs;
                        gs.position = stop.value("position", 0.0f);
                        std::string color = stop.value("color", std::string("#FFFFFF"));
                        gs.color = parse_color_string(color, 0xFFFFFFFF);
                        data.fill.gradient.stops.push_back(gs);
                    }
                }
            }
            else if (shape.contains("fill"))
            {
                const auto& fill = shape["fill"];
                std::string color = fill.value("color", std::string());
                if (!color.empty())
                {
                    data.fill.color = parse_color_string(color, data.fill.color);
                    data.fill.enabled = true;
                }
            }
            else if (shape.contains("color"))
            {
                std::string color = shape["color"].get<std::string>();
                data.fill.color = parse_color_string(color, data.fill.color);
                data.fill.enabled = true;
            }

            if (shape.contains("stroke"))
            {
                const auto& stroke = shape["stroke"];
                data.stroke.enabled = true;
                data.stroke.thickness = stroke.value("thickness", data.stroke.thickness);
                std::string color = stroke.value("color", std::string());
                if (!color.empty())
                {
                    data.stroke.color =
                        parse_color_string(color, data.stroke.color);
                }
                
                // Parse dash
                if (stroke.contains("dash"))
                {
                    const auto& dash = stroke["dash"];
                    data.stroke.dash.enabled = true;
                    data.stroke.dash.length = dash.value("length", data.stroke.dash.length);
                    data.stroke.dash.gap = dash.value("gap", data.stroke.dash.gap);
                    data.stroke.dash.lengthIsPercentage = dash.value("isPercentage", false);
                }
                
                // Parse trimPath for stroke
                if (stroke.contains("trimPath"))
                {
                    const auto& trim = stroke["trimPath"];
                    data.stroke.trimPath.enabled = true;
                    data.stroke.trimPath.start = trim.value("start", data.stroke.trimPath.start);
                    data.stroke.trimPath.end = trim.value("end", data.stroke.trimPath.end);
                    data.stroke.trimPath.offset = trim.value("offset", data.stroke.trimPath.offset);
                    data.stroke.trimPath.mode = trim.value("mode", data.stroke.trimPath.mode);
                }
            }
            
            // Parse feather (can be on fill or shape)
            if (shape.contains("feather"))
            {
                const auto& feather = shape["feather"];
                data.fill.feather.enabled = true;
                data.fill.feather.strength = feather.value("strength", data.fill.feather.strength);
                data.fill.feather.offsetX = feather.value("offsetX", data.fill.feather.offsetX);
                data.fill.feather.offsetY = feather.value("offsetY", data.fill.feather.offsetY);
                data.fill.feather.inner = feather.value("inner", false);
            }

            doc.shapes.push_back(data);
        }
    };

    if (json.contains("shapes"))
    {
        parse_shape_array(json["shapes"]);
    }
    if (json.contains("rectangles"))
    {
        parse_shape_array(json["rectangles"]);
    }

    // Parse custom paths
    if (json.contains("customPaths"))
    {
        for (const auto& pathJson : json["customPaths"])
        {
            CustomPathData pathData;
            pathData.isClosed = pathJson.value("isClosed", true);
            pathData.fillEnabled = pathJson.value("fillEnabled", false);
            pathData.strokeEnabled = pathJson.value("strokeEnabled", false);
            
            // Check for gradient
            if (pathJson.contains("gradient"))
            {
                pathData.hasGradient = true;
                const auto& gradJson = pathJson["gradient"];
                pathData.gradient.type = gradJson.value("type", std::string("radial"));
                
                if (gradJson.contains("stops"))
                {
                    for (const auto& stop : gradJson["stops"])
                    {
                        GradientStop gs;
                        gs.position = stop.value("position", 0.0f);
                        std::string color = stop.value("color", std::string("#FFFFFF"));
                        gs.color = parse_color_string(color, 0xFFFFFFFF);
                        pathData.gradient.stops.push_back(gs);
                    }
                }
            }
            else if (pathJson.contains("fillColor"))
            {
                std::string colorStr = pathJson["fillColor"].get<std::string>();
                pathData.fillColor = parse_color_string(colorStr, 0xFFFFFFFF);
            }
            
            if (pathJson.contains("strokeColor"))
            {
                std::string colorStr = pathJson["strokeColor"].get<std::string>();
                pathData.strokeColor = parse_color_string(colorStr, 0xFF000000);
            }
            pathData.strokeThickness = pathJson.value("strokeThickness", 1.0f);
            
            if (pathJson.contains("vertices"))
            {
                for (const auto& vertexJson : pathJson["vertices"])
                {
                    PathVertexData vertexData;
                    std::string typeStr = vertexJson.value("type", std::string("straight"));
                    vertexData.type = (typeStr == "cubic") ? VertexType::cubic : VertexType::straight;
                    vertexData.x = vertexJson.value("x", 0.0f);
                    vertexData.y = vertexJson.value("y", 0.0f);
                    vertexData.radius = vertexJson.value("radius", 0.0f);
                    vertexData.inRotation = vertexJson.value("inRotation", 0.0f);
                    vertexData.inDistance = vertexJson.value("inDistance", 0.0f);
                    vertexData.outRotation = vertexJson.value("outRotation", 0.0f);
                    vertexData.outDistance = vertexJson.value("outDistance", 0.0f);
                    pathData.vertices.push_back(vertexData);
                }
            }
            
            doc.customPaths.push_back(pathData);
        }
    }

    // Parse texts
    if (json.contains("texts"))
    {
        for (const auto& text : json["texts"])
        {
            TextData textData;
            textData.content = text.value("content", textData.content);
            textData.x = text.value("x", textData.x);
            textData.y = text.value("y", textData.y);
            textData.width = text.value("width", textData.width);
            textData.height = text.value("height", textData.height);
            textData.align = text.value("align", textData.align);
            textData.sizing = text.value("sizing", textData.sizing);
            textData.overflow = text.value("overflow", textData.overflow);
            textData.wrap = text.value("wrap", textData.wrap);
            textData.verticalAlign = text.value("verticalAlign", textData.verticalAlign);
            textData.paragraphSpacing = text.value("paragraphSpacing", textData.paragraphSpacing);
            textData.fitFromBaseline = text.value("fitFromBaseline", textData.fitFromBaseline);
            
            if (text.contains("style"))
            {
                const auto& style = text["style"];
                textData.style.fontFamily = style.value("fontFamily", textData.style.fontFamily);
                textData.style.fontSize = style.value("fontSize", textData.style.fontSize);
                textData.style.fontWeight = style.value("fontWeight", textData.style.fontWeight);
                textData.style.fontWidth = style.value("fontWidth", textData.style.fontWidth);
                textData.style.fontSlant = style.value("fontSlant", textData.style.fontSlant);
                textData.style.lineHeight = style.value("lineHeight", textData.style.lineHeight);
                textData.style.letterSpacing = style.value("letterSpacing", textData.style.letterSpacing);
                
                // Parse text fill color
                if (style.contains("color"))
                {
                    std::string color = style["color"].get<std::string>();
                    textData.style.color = parse_color_string(color, textData.style.color);
                }
                
                // Parse text stroke (outline)
                textData.style.hasStroke = style.value("hasStroke", textData.style.hasStroke);
                if (textData.style.hasStroke)
                {
                    textData.style.strokeThickness = style.value("strokeThickness", textData.style.strokeThickness);
                    if (style.contains("strokeColor"))
                    {
                        std::string strokeColor = style["strokeColor"].get<std::string>();
                        textData.style.strokeColor = parse_color_string(strokeColor, textData.style.strokeColor);
                    }
                }
            }
            
            doc.texts.push_back(textData);
        }
    }

    // Parse animations
    if (json.contains("animations"))
    {
        for (const auto& anim : json["animations"])
        {
            AnimationData animData;
            animData.name = anim.value("name", animData.name);
            animData.fps = anim.value("fps", animData.fps);
            animData.duration = anim.value("duration", animData.duration);
            animData.loop = anim.value("loop", animData.loop);
            
            if (anim.contains("yKeyframes"))
            {
                for (const auto& kf : anim["yKeyframes"])
                {
                    KeyFrameData keyframe;
                    keyframe.frame = kf.value("frame", keyframe.frame);
                    keyframe.value = kf.value("value", keyframe.value);
                    animData.yKeyframes.push_back(keyframe);
                }
            }
            
            if (anim.contains("scaleKeyframes"))
            {
                for (const auto& kf : anim["scaleKeyframes"])
                {
                    KeyFrameData keyframe;
                    keyframe.frame = kf.value("frame", keyframe.frame);
                    keyframe.value = kf.value("value", keyframe.value);
                    animData.scaleKeyframes.push_back(keyframe);
                }
            }
            
            if (anim.contains("opacityKeyframes"))
            {
                for (const auto& kf : anim["opacityKeyframes"])
                {
                    KeyFrameData keyframe;
                    keyframe.frame = kf.value("frame", keyframe.frame);
                    keyframe.value = kf.value("value", keyframe.value);
                    animData.opacityKeyframes.push_back(keyframe);
                }
            }
            
            doc.animations.push_back(animData);
        }
    }
    
    // Parse state machines - FULL (inputs, layers, states, transitions)
    if (json.contains("stateMachines"))
    {
        for (const auto& sm : json["stateMachines"])
        {
            StateMachineData smData;
            smData.name = sm.value("name", smData.name);
            
            // Parse inputs
            if (sm.contains("inputs"))
            {
                for (const auto& input : sm["inputs"])
                {
                    StateMachineInputData inputData;
                    inputData.name = input.value("name", inputData.name);
                    inputData.type = input.value("type", inputData.type);
                    inputData.defaultValue = input.value("value", input.value("defaultValue", inputData.defaultValue));
                    smData.inputs.push_back(inputData);
                }
            }
            
            // Parse layers
            if (sm.contains("layers"))
            {
                for (const auto& layer : sm["layers"])
                {
                    LayerData layerData;
                    layerData.name = layer.value("name", layerData.name);
                    
                    // Parse states
                    if (layer.contains("states"))
                    {
                        for (const auto& state : layer["states"])
                        {
                            StateData stateData;
                            stateData.name = state.value("name", stateData.name);
                            stateData.type = state.value("type", stateData.type);
                            stateData.animationName = state.value("animationName", stateData.animationName);
                            layerData.states.push_back(stateData);
                        }
                    }
                    
                    // Parse transitions
                    if (layer.contains("transitions"))
                    {
                        for (const auto& trans : layer["transitions"])
                        {
                            TransitionData transData;
                            transData.from = trans.value("from", transData.from);
                            transData.to = trans.value("to", transData.to);
                            transData.duration = trans.value("duration", transData.duration);
                            
                            // Parse conditions
                            if (trans.contains("conditions"))
                            {
                                for (const auto& cond : trans["conditions"])
                                {
                                    TransitionConditionData condData;
                                    condData.input = cond.value("input", condData.input);
                                    condData.op = cond.value("op", condData.op);
                                    condData.value = cond.value("value", condData.value);
                                    transData.conditions.push_back(condData);
                                }
                            }
                            
                            layerData.transitions.push_back(transData);
                        }
                    }
                    
                    smData.layers.push_back(layerData);
                }
            }
            
            doc.stateMachines.push_back(smData);
        }
    }
    
    if (json.contains("dataConverters") && json["dataConverters"].is_array())
    {
        for (const auto& converterJson : json["dataConverters"])
        {
            doc.dataConverters.push_back(parse_data_converter_json(converterJson));
        }
    }

    auto parse_data_bind_array = [&](const nlohmann::json& array) {
        for (const auto& bindJson : array)
        {
            doc.dataBinds.push_back(parse_data_bind_json(bindJson));
        }
    };

    if (json.contains("dataBinds") && json["dataBinds"].is_array())
    {
        parse_data_bind_array(json["dataBinds"]);
    }

    if (json.contains("dataBindings") && json["dataBindings"].is_array())
    {
        parse_data_bind_array(json["dataBindings"]);
    }

    // Parse constraints
    if (json.contains("constraints"))
    {
        for (const auto& constraint : json["constraints"])
        {
            ConstraintData constData;
            constData.type = constraint.value("type", constData.type);
            constData.targetId = constraint.value("targetId", constData.targetId);
            constData.strength = constraint.value("strength", constData.strength);
            doc.constraints.push_back(constData);
        }
    }
    
    // Parse events (NEW - Casino Slots support)
    if (json.contains("events"))
    {
        for (const auto& event : json["events"])
        {
            EventData eventData;
            eventData.name = event.value("name", eventData.name);
            eventData.type = event.value("type", eventData.type);
            eventData.assetId = event.value("assetId", eventData.assetId);
            doc.events.push_back(eventData);
        }
    }
    
    // Parse bones (NEW - Casino Slots support)
    if (json.contains("bones"))
    {
        for (const auto& bone : json["bones"])
        {
            BoneData boneData;
            boneData.name = bone.value("name", boneData.name);
            boneData.type = bone.value("type", boneData.type);
            boneData.x = bone.value("x", boneData.x);
            boneData.y = bone.value("y", boneData.y);
            boneData.length = bone.value("length", boneData.length);
            doc.bones.push_back(boneData);
        }
    }
    
    // Parse skins (NEW - Casino Slots support)
    if (json.contains("skins"))
    {
        for (const auto& skin : json["skins"])
        {
            SkinData skinData;
            skinData.xx = skin.value("xx", skinData.xx);
            skinData.yx = skin.value("yx", skinData.yx);
            skinData.xy = skin.value("xy", skinData.xy);
            skinData.yy = skin.value("yy", skinData.yy);
            skinData.tx = skin.value("tx", skinData.tx);
            skinData.ty = skin.value("ty", skinData.ty);
            doc.skins.push_back(skinData);
        }
    }

    // Legacy support & content migration:
    // 1. If no artboards parsed yet, create one from legacy fields
    // 2. If artboards exist but root-level content was parsed, add to first artboard
    if (doc.artboards.empty())
    {
        ArtboardData legacyArtboard;
        
        // Get artboard info from legacy location
        if (json.contains("artboard"))
        {
            const auto& artboardJson = json["artboard"];
            legacyArtboard.name = artboardJson.value("name", std::string("Artboard"));
            legacyArtboard.width = artboardJson.value("width", 400.0f);
            legacyArtboard.height = artboardJson.value("height", 300.0f);
        }
        
        // Move content from doc to artboard
        legacyArtboard.shapes = std::move(doc.shapes);
        legacyArtboard.customPaths = std::move(doc.customPaths);
        legacyArtboard.texts = std::move(doc.texts);
        legacyArtboard.animations = std::move(doc.animations);
        legacyArtboard.stateMachines = std::move(doc.stateMachines);
        legacyArtboard.dataConverters = std::move(doc.dataConverters);
        legacyArtboard.dataBinds = std::move(doc.dataBinds);
        legacyArtboard.constraints = std::move(doc.constraints);
        legacyArtboard.events = std::move(doc.events);
        legacyArtboard.bones = std::move(doc.bones);
        legacyArtboard.skins = std::move(doc.skins);
        
        doc.artboards.push_back(std::move(legacyArtboard));
    }
    else
    {
        // Artboards array format was used, but root-level content may still exist
        // Add any root-level content to the first artboard
        if (!doc.artboards.empty())
        {
            auto& firstArtboard = doc.artboards[0];
            
            // Append root-level content to first artboard
            firstArtboard.shapes.insert(firstArtboard.shapes.end(),
                std::make_move_iterator(doc.shapes.begin()),
                std::make_move_iterator(doc.shapes.end()));
            firstArtboard.customPaths.insert(firstArtboard.customPaths.end(),
                std::make_move_iterator(doc.customPaths.begin()),
                std::make_move_iterator(doc.customPaths.end()));
            firstArtboard.texts.insert(firstArtboard.texts.end(),
                std::make_move_iterator(doc.texts.begin()),
                std::make_move_iterator(doc.texts.end()));
            firstArtboard.dataConverters.insert(firstArtboard.dataConverters.end(),
                std::make_move_iterator(doc.dataConverters.begin()),
                std::make_move_iterator(doc.dataConverters.end()));
            firstArtboard.dataBinds.insert(firstArtboard.dataBinds.end(),
                std::make_move_iterator(doc.dataBinds.begin()),
                std::make_move_iterator(doc.dataBinds.end()));
            firstArtboard.constraints.insert(firstArtboard.constraints.end(),
                std::make_move_iterator(doc.constraints.begin()),
                std::make_move_iterator(doc.constraints.end()));
            firstArtboard.events.insert(firstArtboard.events.end(),
                std::make_move_iterator(doc.events.begin()),
                std::make_move_iterator(doc.events.end()));
            firstArtboard.bones.insert(firstArtboard.bones.end(),
                std::make_move_iterator(doc.bones.begin()),
                std::make_move_iterator(doc.bones.end()));
            firstArtboard.skins.insert(firstArtboard.skins.end(),
                std::make_move_iterator(doc.skins.begin()),
                std::make_move_iterator(doc.skins.end()));
                
            // Clear doc-level vectors
            doc.shapes.clear();
            doc.customPaths.clear();
            doc.texts.clear();
            doc.dataConverters.clear();
            doc.dataBinds.clear();
            doc.constraints.clear();
            doc.events.clear();
            doc.bones.clear();
            doc.skins.clear();
        }
    }

    return doc;
}
} // namespace rive_converter
