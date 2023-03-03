#include "Game/Controller.hpp"
#include "Engine/Core/EngineCommon.hpp"

Controller::Controller(World* world)
	:m_world(world)
{
}

Controller::~Controller()
{

}

void Controller::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}

void Controller::Possess(Entity* possessedEntity)
{
	m_entityPossessed = possessedEntity;
}

Entity* Controller::GetPossessedEntity() const
{
	return m_entityPossessed;
}

