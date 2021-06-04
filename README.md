# Quiver
#### Simple, Data-Oriented, Single-Header ECS using C++20

-----

Quiver provides a simple interface for handling objects, their components, and how they interact with systems, without
many of the pitfalls that befall implementations using object-oriented design. There are no virtual functions calls,
only one instance of inheritance, and way too much caching and data duplication, focusing on speed and memory coherence
rather than memory usage or binary size

## Examples
### Registering and Using Components
```c++
#include <quiver.h>

struct Transform {
    float x, y, z;
};

struct RigidBody {
    // Implementation details
};

int main() {
    // List components to register in template, can do each seperately or together
    qv::World::registerComponent<Transform, Rigidbody>();
    
    // Provides intuitive OOP interface to underlying object
    qv::Entity entity;
    
    // Adds a default-initialized component, does not return component
    entity.addComponent<Transform>();
    
    // Can get component, fails if entity doesn't have component
    entity.getComponent<Transform>().x = 5.0f;
    
    return EXIT_SUCCESS;
}
```

### Registering and Using Systems
```c++
#include <quiver.h>

struct Transform { /* same as above */ };

struct Velocity {
    float x, y, z;
};

// Inherit from qv::System with requested components as template parameters
struct DiscreteVelocitySystem : qv::System<Transform, Velocity> {
    // Can create arbitrary functions, nothing called by Quiver
    static void update() {
        // getComponents() returns a view over a range of tuples containing the requested
        // components and an EntityHandle, so structured binding is used to grab each individually
        for (auto& [transform, velocity, handle] : getComponents()) {
            transform.x += velocity.x * dt;
            transform.y += velocity.y * dt;
            transform.z += velocity.z * dt;
        }
    }
    
    static constexpr float dt = 0.01f
};

int main() {
    qv::World::RegisterComponent<Transform, Velocity>();
    
    // Calls underlying qv::System to register with qv::World
    DiscreteVelocitySystem::registerSystem();
    
    qv::Entity entity;
    entity.addComponent<Transform>();
    entity.addComponent<Velocity>();
    
    // getComponents() in update function will now return references to needed pieces of entity
    DiscreteVelocitySystem::update();
    
    return EXIT_SUCCESS;
}
```