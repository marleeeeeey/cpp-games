#include "Paddle.h"
#include "IStaticObject.h"
#include "IDynamicObject.h"
#include "IHaveParent.h"

Paddle::Paddle()
{
    m_offset = 0;
    m_paddleState = PaddleState::Stop;
}

void Paddle::calculateOffset( std::optional<sf::Event> event, sf::Time elapsedTime )
{
    if ( event && event.value().type == sf::Event::EventType::KeyPressed &&
         event.value().key.code == sf::Keyboard::Key::Left && m_paddleState != PaddleState::MoveLeft )
    {
        m_paddleState = PaddleState::MoveLeft;
    }
    else if (
        event && event.value().type == sf::Event::EventType::KeyReleased &&
        event.value().key.code == sf::Keyboard::Key::Left && m_paddleState == PaddleState::MoveLeft )
    {
        m_paddleState = PaddleState::Stop;
    }
    else if (
        event && event.value().type == sf::Event::EventType::KeyPressed &&
        event.value().key.code == sf::Keyboard::Key::Right && m_paddleState != PaddleState::MoveRight )
    {
        m_paddleState = PaddleState::MoveRight;
    }
    else if (
        event && event.value().type == sf::Event::EventType::KeyReleased &&
        event.value().key.code == sf::Keyboard::Key::Right && m_paddleState == PaddleState::MoveRight )
    {
        m_paddleState = PaddleState::Stop;
    }
    else if (
        event && event.value().type == sf::Event::EventType::KeyReleased &&
        event.value().key.code == sf::Keyboard::Key::Space && m_paddleState != PaddleState::Attack )
    {
        m_paddleState = PaddleState::Attack;
    }

    static const auto speed_pxps = getConfig<float, "game.objects.paddle.speed">();
    float elapsedSec = elapsedTime.asSeconds();
    float absOffset = speed_pxps * elapsedSec;
    static const auto damping = getConfig<float, "game.objects.paddle.damping">();
    float absDampingOffset = absOffset * damping;

    switch ( m_paddleState )
    {
    case PaddleState::Stop:
        if ( m_offset > 0 )
            m_offset -= absDampingOffset;
        else if ( m_offset < 0 )
            m_offset += absDampingOffset;
        if ( m_offset != 0.0f && abs( m_offset ) < absDampingOffset )
            m_offset = 0;
        break;
    case PaddleState::MoveLeft:
        m_offset = -absOffset;
        break;
    case PaddleState::MoveRight:
        m_offset = absOffset;
        break;
    case PaddleState::Attack:
        // TODO
        break;
    }
}

void Paddle::calcState( std::optional<sf::Event> event, sf::Time elapsedTime )
{
    sf::Vector2f pos01 = state().getPos();
    calculateOffset( event, elapsedTime );
    auto pos = state().getPos();
    pos.x += m_offset;
    state().setPos( pos );
    if ( haveCollisions( m_collisionWalls ) )
    {
        m_offset = 0;
        restoreState();
    }
    else
    {
        saveState();
        m_collisionWalls.clear();
    }

    sf::Vector2f pos02 = state().getPos();
    sf::Vector2f shift = pos02 - pos01;

    auto size = state().getSize();
    if ( m_bonusType && m_bonusType.value() == BonusType::LongPlate )
    {
        if ( !m_originalSize )
            m_originalSize = size;

        size = m_originalSize.value();
        auto longSizeFactor = getConfig<float, "game.objects.paddle.longSizeFactor">();
        size.x *= longSizeFactor;
        state().setSize( size );
    }
    else
    {
        if ( m_originalSize )
            state().setSize( m_originalSize.value() );
    }

    for ( const auto& ball : m_magnetBalls )
    {
        auto ballPos = ball->state().getPos();
        ballPos += shift;
        ball->state().setPos( ballPos );
    }
}

void Paddle::draw( sf::RenderWindow& window )
{
    auto shape = state().getCollisionRect();
    shape.setFillColor( sf::Color::Cyan );
    window.draw( shape );

    if ( m_bonusType )
    {
        sf::Text text;
        sf::Font font = getDefaultFont();
        text.setFont( font );
        text.setString( toString( m_bonusType.value() ) );
        text.setFillColor( sf::Color::Blue );
        setTextCenterTo( text, state().getPos() );
        window.draw( text );
    }
}

float getShiftCoef( const std::shared_ptr<IObject>& paddle, const std::shared_ptr<IObject>& obj )
{
    auto ballCenter = obj->state().getPos();
    auto paddleCenter = paddle->state().getPos();
    auto centersShift = ballCenter.x - paddleCenter.x;
    auto halfLength = paddle->state().getSize().x / 2;
    auto result = centersShift / halfLength;
    return result;
}

void Paddle::onBumping( std::vector<Collision>& collisions )
{
    for ( auto collision : collisions )
    {
        auto obj = collision.getObject();
        auto wall = std::dynamic_pointer_cast<IStaticObject>( obj );
        auto ball = std::dynamic_pointer_cast<IDynamicObject>( obj );
        if ( wall )
        {
            m_collisionWalls.insert( obj );
        }
        if ( ball )
        {
            static const auto angleSensivity = getConfig<float, "game.objects.paddle.angle_sensitivity">();
            float angleShift = angleSensivity * getShiftCoef( shared_from_this(), obj );
            rotateDegInPlace( ball->velocity(), angleShift );
            if ( m_bonusType && m_bonusType.value() == BonusType::MagnetPaddle && m_paddleState != PaddleState::Attack )
            {
                auto [_, success] = m_magnetBalls.insert( obj );
                if ( success )
                {
                    auto localBall = std::dynamic_pointer_cast<IHaveParent>( obj );
                    localBall->setParent( shared_from_this() );
                }
            }
            else
            {
                for ( const auto& magnetBall : m_magnetBalls )
                {
                    auto childBall = std::dynamic_pointer_cast<IHaveParent>( magnetBall );
                    childBall->removeParent();
                    // TODO refactor it
                    float maxAngle_deg = 45;
                    float angle = maxAngle_deg * getShiftCoef( shared_from_this(), magnetBall ) - 90;
                    auto dynamicBall = std::dynamic_pointer_cast<IDynamicObject>( magnetBall );
                    setAngle( dynamicBall->velocity(), angle );
                }
                m_magnetBalls.clear();
            }
        }
    }
}

std::optional<BonusType>& Paddle::bonusType()
{
    return m_bonusType;
}

std::shared_ptr<IObject> Paddle::createCopyFromThis()
{
    auto createdObjectPtr = std::make_shared<Paddle>();
    Paddle& createdObject = *createdObjectPtr.get();
    createdObject = *this;
    return createdObjectPtr;
}

std::string Paddle::name()
{
    return "Paddle";
}
