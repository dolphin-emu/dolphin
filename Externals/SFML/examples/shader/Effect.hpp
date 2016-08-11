#ifndef EFFECT_HPP
#define EFFECT_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics.hpp>
#include <cassert>
#include <string>


////////////////////////////////////////////////////////////
// Base class for effects
////////////////////////////////////////////////////////////
class Effect : public sf::Drawable
{
public:

    virtual ~Effect()
    {
    }

    static void setFont(const sf::Font& font)
    {
        s_font = &font;
    }

    const std::string& getName() const
    {
        return m_name;
    }

    void load()
    {
        m_isLoaded = sf::Shader::isAvailable() && onLoad();
    }

    void update(float time, float x, float y)
    {
        if (m_isLoaded)
            onUpdate(time, x, y);
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const
    {
        if (m_isLoaded)
        {
            onDraw(target, states);
        }
        else
        {
            sf::Text error("Shader not\nsupported", getFont());
            error.setPosition(320.f, 200.f);
            error.setCharacterSize(36);
            target.draw(error, states);
        }
    }

protected:

    Effect(const std::string& name) :
    m_name(name),
    m_isLoaded(false)
    {
    }

    static const sf::Font& getFont()
    {
        assert(s_font != NULL);
        return *s_font;
    }

private:

    // Virtual functions to be implemented in derived effects
    virtual bool onLoad() = 0;
    virtual void onUpdate(float time, float x, float y) = 0;
    virtual void onDraw(sf::RenderTarget& target, sf::RenderStates states) const = 0;

private:

    std::string m_name;
    bool m_isLoaded;

    static const sf::Font* s_font;
};

#endif // EFFECT_HPP
