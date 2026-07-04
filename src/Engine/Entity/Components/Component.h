#pragma once

class Entity;

class Component
{
  public:
    virtual ~Component();

    virtual void Init();
    virtual void Update(float deltaTime);
    virtual void Render();

    Entity* GetEntity() const;

  protected:
    friend class Entity;
    void setEntity(Entity* entity);

  private:
    Entity* m_entity = nullptr;
};
