#version 450

// Uniform buffers
layout(binding = 0) uniform camera_params {
    vec3 camera_pos;
    float _pad0;
    mat4 view_matrix;
    mat4 projection_matrix;
    mat4 view_proj_matrix;
} camera;

layout(binding = 1) uniform joint_transforms {
    mat4 jointTransforms[256]; // MAX_JOINTS = 256
} joints;

// Model matrix now comes from push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
} push_constants;

layout(binding = 6) uniform blendshape_params {
    ivec4 countAndFlags;
    vec4 weights[8]; // Support up to 32 blendshapes (8 vec4s * 4 components)
} blendshapes;

// Storage buffer for blendshape data
struct BlendshapeDelta {
    vec4 position;
    vec4 normal;
};

layout(std430, binding = 7) readonly buffer BlendshapeData {
    BlendshapeDelta deltas[];
} blendshapeData;

// Vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in uvec4 aJointIndexes;
layout(location = 4) in vec4 aWeights;

// Outputs to fragment shader
layout(location = 0) out vec3 vs_normal;
layout(location = 1) out vec2 vs_texcoord;
layout(location = 2) out vec3 vs_world_pos;
layout(location = 3) out vec3 vs_eye_dir;

#define MAX_WEIGHTS 4
#define MAX_BLENDSHAPES 32

void main() {
    vec4 position = vec4(aPosition, 1.0);
    vec4 normal = vec4(aNormal, 0.0);

    // Apply blendshapes
    int blendshapeCount = blendshapes.countAndFlags.x;
    if (blendshapeCount > 0) {
        vec3 blendedPositionDelta = vec3(0.0);
        vec3 blendedNormalDelta = vec3(0.0);

        for (int i = 0; i < blendshapeCount && i < MAX_BLENDSHAPES; i++) {
            int vecIndex = i / 4;
            int component = i % 4;
            float weight = blendshapes.weights[vecIndex][component];

            if (weight > 0.001) {
                int deltaIndex = gl_VertexIndex * blendshapeCount + i;

                BlendshapeDelta delta = blendshapeData.deltas[deltaIndex];
                vec3 posDelta = delta.position.xyz;
                vec3 normalDelta = delta.normal.xyz;

                blendedPositionDelta += posDelta * weight;
                blendedNormalDelta += normalDelta * weight;
            }
        }

        position.xyz += blendedPositionDelta;
        normal.xyz += blendedNormalDelta;
    }

    // Apply skinning
    vec4 skinnedPosition = vec4(0.0);
    vec4 skinnedNormal = vec4(0.0);

    for (int i = 0; i < MAX_WEIGHTS; i++) {
        uint jointIndex = aJointIndexes[i];
        mat4 jointTransform = joints.jointTransforms[jointIndex];

        vec4 posePosition = jointTransform * position;
        skinnedPosition += posePosition * aWeights[i];

        vec4 worldNormal = jointTransform * normal;
        skinnedNormal += worldNormal * aWeights[i];
    }
    skinnedPosition.w = 1.0;

    position = skinnedPosition;
    normal = skinnedNormal;

    position = vec4(aPosition, 1.0);
    normal = vec4(aNormal, 0.0);

    // Transform to world space
    vec4 worldPos = push_constants.model * position;
    vs_world_pos = worldPos.xyz;

    // Transform normal to world space
    vs_normal = normalize(mat3(push_constants.model) * normal.xyz);

    // Pass through texture coordinates
    vs_texcoord = aTexCoord;

    // Calculate eye direction
    vs_eye_dir = normalize(camera.camera_pos - vs_world_pos);

    // Final position in clip space
    gl_Position = camera.view_proj_matrix * worldPos;
}