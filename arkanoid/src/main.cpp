#include "ObjectFactory.h"
#include "World.h"
#include "HelperFunctions.h"
#include "LevelGenerator.h"

int main()
{
    MY_LOG( info, "This is an info message" );
    MY_LOG_FMT( info, "Hello, {}", "world" );

    sf::Vector2f windowSize = { 600, 800 };

    MY_LOG_VAR( info, windowSize );

    auto objectFactory = std::make_shared<ObjectFactory>();
    auto levelGenerator = std::make_shared<LevelGenerator>( objectFactory, windowSize );
    auto world = std::make_shared<World>( objectFactory, levelGenerator, windowSize );
    auto videoMode = sf::VideoMode( static_cast<unsigned>( windowSize.x ), static_cast<unsigned>( windowSize.y ) );
    sf::RenderWindow window( videoMode, "Arkanoid" );
    window.setFramerateLimit( 60 );

    std::ignore = ImGui::SFML::Init( window );
    sf::Clock deltaClock;
    auto lastTime = deltaClock.getElapsedTime();

    while ( window.isOpen() )
    {
        std::optional<sf::Event> optEvent;
        sf::Event curEvent{};

        while ( window.pollEvent( curEvent ) )
        {
            ImGui::SFML::ProcessEvent( curEvent );
            if ( curEvent.type == sf::Event::Closed || hf::isKeyPressed( curEvent, sf::Keyboard::Escape ) )
            {
                window.close();
            }
            optEvent = curEvent;
        }

        auto currentTime = deltaClock.getElapsedTime();
        auto timeDiff = currentTime - lastTime;
        lastTime = currentTime;

        //// ImGui window
        // ImGui::Begin( "Debug Window" );
        // ImGui::Button( "Click me 2" );
        // ImGui::End();

        // Game logic
        ImGui::SFML::Update( window, timeDiff );
        world->updateState( optEvent, timeDiff );

        // Game rendering
        window.clear();
        world->draw( window );
        ImGui::SFML::Render( window );
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}
