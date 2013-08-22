#ifndef _CIFACE_SDL_H_
#define _CIFACE_SDL_H_

#include "../Device.h"

#include <list>

#include <SDL.h>

#if SDL_VERSION_ATLEAST(1, 3, 0)
	#define USE_SDL_HAPTIC
#endif

#ifdef USE_SDL_HAPTIC
	#include <SDL_haptic.h>
	#define SDL_INIT_FLAGS	SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC
#else
	#define SDL_INIT_FLAGS	SDL_INIT_JOYSTICK
#endif

namespace ciface
{
namespace SDL
{

void Init( std::vector<Core::Device*>& devices );

class Joystick : public Core::Device
{
private:

#ifdef USE_SDL_HAPTIC
	struct EffectIDState
	{
		EffectIDState() : effect(SDL_HapticEffect()), id(-1), changed(false) {}

		SDL_HapticEffect	effect;
		int					id;
		bool				changed;
	};
#endif

	class Button : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Button(u8 index, SDL_Joystick* js) : m_js(js), m_index(index) {}
		ControlState GetState() const;
	private:
		 SDL_Joystick* const m_js;
		 const u8 m_index;
	};

	class Axis : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Axis(u8 index, SDL_Joystick* js, Sint16 range) : m_js(js), m_range(range), m_index(index) {}
		ControlState GetState() const;
	private:
		SDL_Joystick* const m_js;
		const Sint16 m_range;
		const u8 m_index;
	};

	class Hat : public Input
	{
	public:
		std::string GetName() const;
		Hat(u8 index, SDL_Joystick* js, u8 direction) : m_js(js), m_direction(direction), m_index(index) {}
		ControlState GetState() const;
	private:
		SDL_Joystick* const m_js;
		const u8 m_direction;
		const u8 m_index;
	};

#ifdef USE_SDL_HAPTIC
	class ConstantEffect : public Output
	{
	public:
		std::string GetName() const;
		ConstantEffect(EffectIDState& effect) : m_effect(effect) {}
		void SetState(const ControlState state);
	private:
		EffectIDState& m_effect;
	};

	class RampEffect : public Output
	{
	public:
		std::string GetName() const;
		RampEffect(EffectIDState& effect) : m_effect(effect) {}
		void SetState(const ControlState state);
	private:
		EffectIDState& m_effect;
	};

	class SineEffect : public Output
	{
	public:
		std::string GetName() const;
		SineEffect(EffectIDState& effect) : m_effect(effect) {}
		void SetState(const ControlState state);
	private:
		EffectIDState& m_effect;
	};

#ifdef SDL_HAPTIC_SQUARE
	class SquareEffect : public Output
	{
	public:
		std::string GetName() const;
		SquareEffect(EffectIDState& effect) : m_effect(effect) {}
		void SetState(const ControlState state);
	private:
		EffectIDState& m_effect;
	};
#endif // defined(SDL_HAPTIC_SQUARE)

	class TriangleEffect : public Output
	{
	public:
		std::string GetName() const;
		TriangleEffect(EffectIDState& effect) : m_effect(effect) {}
		void SetState(const ControlState state);
	private:
		EffectIDState& m_effect;
	};
#endif

public:
	bool UpdateInput();
	bool UpdateOutput();

	Joystick(SDL_Joystick* const joystick, const int sdl_index, const unsigned int index);
	~Joystick();

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	SDL_Joystick* const			m_joystick;
	const int					m_sdl_index;
	const unsigned int			m_index;

#ifdef USE_SDL_HAPTIC
	std::list<EffectIDState>	m_state_out;
	SDL_Haptic*					m_haptic;
#endif
};

}
}

#endif
