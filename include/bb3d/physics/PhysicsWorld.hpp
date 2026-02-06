#pragma once

#include "bb3d/core/Base.hpp"
#include <glm/glm.hpp>

namespace bb3d {

    class Scene;
    class Entity;

    /** @brief Résultat détaillé d'un test de collision par rayon (Raycast). */
    struct RaycastResult {
        bool hit = false;              ///< true si un objet a été touché.
        float fraction = 0.0f;         ///< Distance normalisée [0-1] le long du rayon.
        glm::vec3 position = { 0.0f, 0.0f, 0.0f }; ///< Point d'impact dans le monde.
        glm::vec3 normal = { 0.0f, 0.0f, 0.0f };   ///< Normale de la surface au point d'impact.
        uint32_t bodyID = 0xFFFFFFFF;  ///< Identifiant du corps physique touché.
    };

    /**
     * @brief Gère la simulation physique 3D via le moteur Jolt Physics.
     * 
     * Cette classe est responsable de :
     * - L'initialisation du moteur Jolt (Allocateurs, JobSystem interne).
     * - La synchronisation des positions entre EnTT (TransformComponent) et Jolt.
     * - La gestion des corps rigides (RigidBody) et des contrôleurs de personnages (CharacterVirtual).
     */
    class PhysicsWorld {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        /** @brief Initialise Jolt et configure les couches de collision par défaut. */
        void init();

        /** 
         * @brief Avance la simulation d'un pas (Step).
         * @param deltaTime Temps écoulé.
         * @param scene Scène contenant les composants physiques à synchroniser.
         */
        void update(float deltaTime, Scene& scene);

        /** @brief Arrête le moteur physique et libère la mémoire. */
        void shutdown();

        /** @brief Copie les positions de Jolt vers les TransformComponents de la scène. */
        void syncTransforms(Scene& scene);

        /** 
         * @brief Crée un corps physique Jolt pour une entité possédant un RigidBodyComponent. 
         * @warning Pour les MeshColliders, cette méthode doit être appelée AVANT Mesh::releaseCPUData().
         */
        void createRigidBody(Entity entity);

        /** @brief Crée un CharacterController virtuel (idéal pour le joueur). */
        void createCharacterController(Entity entity);

        /** 
         * @brief Lance un rayon dans la scène physique.
         * @param origin Point de départ.
         * @param direction Vecteur directionnel (normalisé).
         * @param maxDistance Distance maximale du test.
         */
        RaycastResult raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance);

    private:
        struct Impl; ///< PIMPL pour cacher les headers Jolt de l'API publique.
        Scope<Impl> m_impl;
    };

} // namespace bb3d