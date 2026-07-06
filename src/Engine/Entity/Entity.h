#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <typeindex>

#include "Components/Component.h"

class Entity
{
  public:
    Entity();
    ~Entity();

    template <typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from component");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* componentPtr = component.get();
        component->setEntity(this);

        m_components[typeid(T)] = std::move(component);
        return componentPtr;
    }

    template <typename T>
    T* GetComponent()
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive form component");

        auto it = m_components.find(typeid(T));
        if (it != m_components.end())
            return static_cast<T*>(it->second.get());

        for (auto& pair : m_components)
        {
            if (T* component = dynamic_cast<T*>(pair.second.get()))
                return component;
        }
        return nullptr;
    }

    template <typename T>
    const T* GetComponent() const
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive form component");

        auto it = m_components.find(typeid(T));
        if (it != m_components.end())
            return static_cast<const T*>(it->second.get());

        for (const auto& pair : m_components)
        {
            if (const T* component = dynamic_cast<const T*>(pair.second.get()))
                return component;
        }
        return nullptr;
    }

    template <typename T>
    void RemoveComponent()
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive form component");
        m_components.erase(typeid(T));
    }

    template <typename T>
    bool HasComponent()
    {
        if (m_components.find(typeid(T)) != m_components.end())
            return true;

        for (const auto& pair : m_components)
        {
            if (dynamic_cast<T*>(pair.second.get()))
                return true;
        }
        return false;
    }

    void Init();
    void Update(float deltaTime);
    void Render();

  public:
    std::string m_name;

  private:
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_components;
};
