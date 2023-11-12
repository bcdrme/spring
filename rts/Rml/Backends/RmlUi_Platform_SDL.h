/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019-2023 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef RMLUI_BACKENDS_PLATFORM_SDL_H
#define RMLUI_BACKENDS_PLATFORM_SDL_H

#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Types.h>
#include <SDL.h>
#include "Game/UI/MouseHandler.h"
#include "lib/RmlSolLua/src/TranslationTable.h"

class SystemInterface_SDL : public Rml::SystemInterface
{
public:
	SystemInterface_SDL();
	~SystemInterface_SDL();

	// Optionally, provide or change the window to be used for setting the mouse cursors.
	void SetWindow(SDL_Window *window);

	int TranslateString(Rml::String &translated, const Rml::String &input) override;
	virtual bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

	// -- Inherited from Rml::SystemInterface  --

	double GetElapsedTime() override;

	void SetMouseCursor(const Rml::String &cursor_name) override;

	void SetClipboardText(const Rml::String &text) override;
	void GetClipboardText(Rml::String &text) override;

	void SetTranslationTable(TranslationTable *tt) { translationTable = tt; };

private:
	SDL_Window *window = nullptr;

	TranslationTable *translationTable = nullptr;
};

namespace RmlSDL
{

	// Applies input on the context based on the given SDL event.
	// @return True if the event is still propagating, false if it was handled by the context.
	bool InputEventHandler(Rml::Context *context, const SDL_Event &ev);
	bool EventKeyDown(Rml::Context *context, Rml::Input::KeyIdentifier key);
	bool EventKeyUp(Rml::Context *context, Rml::Input::KeyIdentifier key);
	bool EventTextInput(Rml::Context *context, const std::string& text);

	// Converts the SDL key to RmlUi key.
	Rml::Input::KeyIdentifier ConvertKey(int sdl_key);

	// Converts the SDL mouse button to RmlUi mouse button.
	int ConvertMouseButton(int sdl_mouse_button);

	// Returns the active RmlUi key modifier state.
	int GetKeyModifierState();

} // namespace RmlSDL

#endif