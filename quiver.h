//
// Created by jack on 5/31/21.
//

#ifndef QUIVER_QUIVER_H
#define QUIVER_QUIVER_H

#include <iostream>
#include <functional>
#include <vector>
#include <map>
#include <set>
#include <bitset>
#include <cassert>
#include <ranges>
#include <algorithm>

#ifdef QV_DEBUG_VERBOSE
    #define QV_DEBUG
#endif

namespace qv {

#ifdef QV_COMPONENT_BITSET_SIZE
    using ComponentSignature = std::bitset<QV_COMPONENT_BITSET_SIZE>;
    constexpr size_t componentBitsetSize = QV_COMPONENT_BITSET_SIZE;
#else
    using ComponentSignature = std::bitset<64>;
    constexpr size_t componentBitsetSize = 64;
#endif

    using EntityHandle = size_t;

    template<typename Component>
    struct Registrar {
        static inline ComponentSignature signature;
        static inline size_t signatureBit;
        static inline std::vector<Component> components;
        static inline std::map<EntityHandle, size_t> handleMap;
        static inline std::map<size_t, EntityHandle> reverseMap;

        static void addComponent(Component component, EntityHandle handle) {
            components.template emplace_back(std::move(component));
            handleMap.template emplace(handle, components.size() - 1);
            reverseMap.template emplace(components.size() - 1, handle);
        }

        static void createComponent(EntityHandle handle) {
            addComponent(Component{}, handle);
        }

        static void removeComponent(EntityHandle handle) {
            auto index = handleMap.at(handle);
            std::swap(components.back(), components.at(index));
            auto otherHandle = reverseMap.at(components.size() - 1);
            handleMap.at(otherHandle) = index;
            reverseMap.at(index) = otherHandle;

            components.pop_back();
            handleMap.erase(handle);
            reverseMap.erase(components.size());
        }

        static Component& getComponent(EntityHandle handle) {
            return components.at(handleMap.at(handle));
        }
    };

    class World {
    public:
        template<typename Component, typename... Components>
        static void registerComponent() {
            Registrar<Component>::signature = ComponentSignature{};
            Registrar<Component>::signature.set(componentId);
            Registrar<Component>::signatureBit = componentId;
            destructors.push_back(&Registrar<Component>::removeComponent);
            systemDescriptors.template emplace_back();

            componentId++;
            if constexpr (sizeof...(Components) != 0) {
                registerComponent<Components...>();
            }
        }

        static EntityHandle createEntity() {
            entitySignatures.emplace(EntityHandle{entityId}, ComponentSignature{});
            entitySystemDescriptors.emplace(EntityHandle{entityId}, std::set<SystemDescriptor*>{});
            return entityId++;
        }

        static void destroyEntity(EntityHandle handle) {
            ComponentSignature& signature = entitySignatures.at(handle);
            for (size_t bit = 0; bit < signature.size(); bit++) {
                if (signature.test(bit)) {
                    destructors.at(bit)(handle);
                }
            }

            for (const auto descriptor : entitySystemDescriptors.at(handle)) {
                descriptor->set->erase(handle);
                descriptor->regenerateComponentList();
            }

            entitySystemDescriptors.erase(handle);
            entitySignatures.erase(handle);
        }

        template<typename Component>
        static void addComponent(EntityHandle handle) {
            entitySignatures.at(handle).set(Registrar<Component>::signatureBit);
            Registrar<Component>::createComponent(handle);

            std::ranges::for_each(
                systemDescriptors.at(Registrar<Component>::signatureBit)
                | std::views::filter([handle](auto& descriptor){
                    return World::compareSignatures(entitySignatures.at(handle), descriptor.signature);
                }),
                [handle](auto& descriptor) {
                    descriptor.set->insert(handle);
                    descriptor.regenerateComponentList();
                    World::entitySystemDescriptors.at(handle).template emplace(&descriptor);
                }
            );
        }

        template<typename Component>
        static void removeComponent(EntityHandle handle) {
            std::ranges::for_each(
                systemDescriptors.at(Registrar<Component>::signatureBit)
                | std::views::filter([handle](auto& descriptor){
                    return World::compareSignatures(entitySignatures.at(handle), descriptor.signature);
                }),
                [handle](auto& descriptor) {
                    descriptor.set->erase(handle);
                    descriptor.regenerateComponentList();
                    World::entitySystemDescriptors.at(handle).erase(&descriptor);
                }
            );

            entitySignatures.at(handle).reset(Registrar<Component>::signatureBit);
            Registrar<Component>::removeComponent(handle);
        }

        template<typename Component, typename... Components>
        static ComponentSignature generateSignature() {
            if constexpr (sizeof...(Components) > 0) {
                return generateSignature<Components...>() | Registrar<Component>::signature;
            } else {
                return Registrar<Component>::signature;
            }
        }

    private:
        struct SystemDescriptor {
            ComponentSignature signature;
            std::set<EntityHandle>* set{nullptr};
            std::function<void()> regenerateComponentList;
        };

        static bool compareSignatures(ComponentSignature entity, ComponentSignature system) {
            return (entity & system) == system;
        }

        static inline size_t componentId = 0;
        static inline size_t entityId = 1; // Uses ID 0 for a null handle

        static inline std::vector<std::function<void(EntityHandle)>> destructors;
        static inline std::vector<std::vector<SystemDescriptor>> systemDescriptors;
        static inline std::map<EntityHandle, ComponentSignature> entitySignatures;
        static inline std::map<EntityHandle, std::set<SystemDescriptor*>> entitySystemDescriptors;

        template<typename...>
        friend class System;
    };

    template<typename... Components>
    class System {
    public:
        static void registerSystem() {
            if (registered) return;

            auto signature = World::generateSignature<Components...>();
            for (size_t bit = 0; bit < signature.size(); bit++) {
                if (signature.test(bit)) {
                    World::systemDescriptors.at(bit).template emplace_back(signature, &entities, regenerateComponentList);
                }
            }
            registered = true;
        }

        static std::vector<std::tuple<Components&..., EntityHandle>>& getComponents() {
            return componentList;
        }

        static void regenerateComponentList() {
            componentList.clear();
            componentList.reserve(entities.size());
            std::ranges::copy(entities | std::views::transform(getComponentTuple), std::back_inserter(componentList));
        }
    private:
        static decltype(auto) getComponentTuple(EntityHandle handle) {
            return std::make_tuple<std::reference_wrapper<Components>..., EntityHandle>(
                    std::ref(Registrar<Components>::getComponent(handle))..., EntityHandle{handle}
            );
        }

        static inline std::set<EntityHandle> entities;
        static inline std::vector<std::tuple<Components&..., EntityHandle>> componentList;
        static inline bool registered = false;
    };

    class Entity {
    public:
        Entity() {
            handle = World::createEntity();
            #ifdef QV_DEBUG_VERBOSE
                std::cout << "Entity Created: " << handle << '\n';
            #endif
        }

        ~Entity() {
            if (handle == 0) return;
            World::destroyEntity(handle);
            #ifdef QV_DEBUG_VERBOSE
                std::cout << "Entity Destroyed: " << handle << '\n';
            #endif
        }

        Entity(const Entity&)=delete;
        Entity& operator=(const Entity&)=delete;

        Entity(Entity&& other) noexcept : handle{other.handle} {
            other.handle = 0;
        };

        Entity& operator=(Entity&& other) noexcept {
            handle = other.handle;
            other.handle = 0;
            return *this;
        };

        template<typename Component>
        void addComponent() {
            #ifdef QV_DEBUG
                assert(!Registrar<Component>::handleMap.contains(handle));
            #endif
            World::addComponent<Component>(handle);
        }

        template<typename Component>
        void removeComponent() {
            #ifdef QV_DEBUG
                assert(Registrar<Component>::handleMap.contains(handle));
            #endif
            World::removeComponent<Component>(handle);
        }

        template<typename Component>
        Component& getComponent() {
            #ifdef QV_DEBUG
                assert(Registrar<Component>::handleMap.contains(handle));
            #endif
            return Registrar<Component>::getComponent(handle);
        }
    private:
        EntityHandle handle;
    };
}

#endif //QUIVER_QUIVER_H