#pragma once

class World;
class Entity;

class Controller
{
	friend class Entity;

public:
	Controller(World* world);
	virtual ~Controller();

	virtual void Update(float deltaSeconds);
	void Possess(Entity* possessedEntity);
	Entity* GetPossessedEntity() const;

protected:
	Entity* m_entityPossessed = nullptr;
	World* m_world = nullptr;
};