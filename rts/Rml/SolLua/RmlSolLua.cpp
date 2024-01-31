﻿// This file is part of the Spring engine (GPL v2 or later), see LICENSE.html

#include "RmlSolLua.h"

#include <sol2/sol.hpp>
#include <RmlUi/Core.h>

#include "bind/bind.h"


namespace Rml::SolLua
{

    SolLuaPlugin* Initialise(sol::state_view* state)
    {
        SolLuaPlugin* slp;
        if (state != nullptr)
        {
            slp = new SolLuaPlugin(*state);
            ::Rml::RegisterPlugin(slp);
            RegisterLua(state, slp);
        }
        return slp;
    }

    SolLuaPlugin* Initialise(sol::state_view* state, const Rml::String& lua_environment_identifier)
    {
        SolLuaPlugin* slp;
        if (state != nullptr)
        {
            slp = new SolLuaPlugin(*state, lua_environment_identifier);
            ::Rml::RegisterPlugin(slp);
            RegisterLua(state, slp);
        }
        return slp;
    }

    void RegisterLua(sol::state_view* state, SolLuaPlugin* slp)
    {
		sol::table namespace_table = state->create_named_table("RmlUi");

        bind_color(namespace_table);
        bind_context(namespace_table, slp);
        bind_datamodel(namespace_table);
        bind_element(namespace_table);
        bind_element_derived(namespace_table);
        bind_element_form(namespace_table);
        bind_document(namespace_table);
        bind_event(namespace_table);
        bind_global(namespace_table, slp);
        bind_vector(namespace_table);
        bind_convert(namespace_table);
    }

} // end namespace Rml::SolLua
