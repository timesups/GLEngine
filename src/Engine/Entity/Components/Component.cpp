#include "Component.h"

#include "../Entity.h"

Component::~Component() = default;

void Component::Init() {}

void Component::Update(float deltaTime) {}

void Component::Render() {}

Entity* Component::GetEntity() const
{
    return m_entity;
}

void Component::setEntity(Entity* entity)
{
    m_entity = entity;
}
