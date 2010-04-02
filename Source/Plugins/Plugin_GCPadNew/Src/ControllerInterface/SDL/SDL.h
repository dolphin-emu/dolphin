#ifndef _CIFACE_SDL_H_
#define _CIFACE_SDL_H_

#include "../ControllerInterface.h"

#ifdef _WIN32
	#include <SDL.h>
#else
	#include <SDL/SDL.h>
#endif

#if SDL_VERSION_ATLEAST(1, 3, 0)
	#define USE_SDL_HAPTIC
#endif

#ifdef USE_SDL_HAPTIC
	#define SDL_INIT_FLAGS	SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC
	#ifdef _WIN32
		#include <SDL_haptic.h>
	#else
		#include <SDL/SDL_haptic.h>
	#endif
#else
	#define SDL_INIT_FLAGS	SDL_INIT_JOYSTICK
#endif

namespace ciface
{
namespace SDL
{

void Init( std::vector<ControllerInterface::Device*>& devices );

class Joystick : public ControllerInterface::Device
{
	friend class ControllerInterface;
	friend class ControllerInterface::ControlReference;

protected:

#ifdef USE_SDL_HAPTIC
	class EffectIDState
	{
		friend class Joystick;
	protected:
		EffectIDState() : id(-1), changed(false) { memset( &effect, 0, sizeof(effect)); }
	protected:
		SDL_HapticEffect	effect;
		int					id;
		bool				changed;
	};
#endif

	class Input : public ControllerInterface::Device::Input
	{
		friend class Joystick;
	protected:
		Input( const unsigned int index ) : m_index(index) {}
		virtual ControlState GetState( SDL_Joystick* const js ) const = 0;
		
		const unsigned int		m_index;
	};

#ifdef USE_SDL_HAPTIC
	class Output : public ControllerInterface::Device::Output
	{
		friend class Joystick;
	protected:
		Output( const size_t index ) : m_index(index) {}
		virtual void SetState( const ControlState state, EffectIDState* const effect ) = 0;
		
		const size_t		m_index;
	};
#endif

	class Button : public Input
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		Button( const unsigned int index ) : Input(index) {}
		ControlState GetState( SDL_Joystick* const js ) const;
	};
	class Axis : public Input
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		Axis( const unsigned int index, const Sint16 range ) : Input(index), m_range(range) {}
		ControlState GetState( SDL_Joystick* const js ) const;
	private:
		const Sint16		m_range;
	};

	class Hat : public Input
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		Hat( const unsigned int index, const unsigned int direction ) : Input(index), m_direction(direction) {}
		ControlState GetState( SDL_Joystick* const js ) const;
	private:
		const unsigned int		m_direction;
	};

#ifdef USE_SDL_HAPTIC
	class ConstantEffect : public Output
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		ConstantEffect( const size_t index ) : Output(index) {}
		void SetState( const ControlState state, EffectIDState* const effect );
	};

	class RampEffect : public Output
	{
		friend class Joystick;
	public:
		std::string GetName() const;
	protected:
		RampEffect( const size_t index ) : Output(index) {}
		void SetState( const ControlState state, EffectIDState* const effect );
	};
#endif

	bool UpdateInput();
	bool UpdateOutput();

	ControlState GetInputState( const ControllerInterface::Device::Input* const input );
	void SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state );

public:
	Joystick( SDL_Joystick* const joystick, const unsigned int index );
	~Joystick();

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	SDL_Joystick* const			m_joystick;
	const unsigned int			m_index;

#ifdef USE_SDL_HAPTIC
	std::vector<EffectIDState>	m_state_out;
	SDL_Haptic*					m_haptic;
#endif
};


}
}

#endif
