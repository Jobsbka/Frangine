#version 450
layout(location = 0) in vec3 fragNormal;
layout(location = 0) out vec4 outColor;
void main() {
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));
    float diff = max(dot(fragNormal, lightDir), 0.0);
    vec3 color = vec3(0.6, 0.5, 0.4) * diff + vec3(0.2);
    outColor = vec4(color, 1.0);
}