#version 110

uniform vec4 uniform_color;
uniform float emission_factor;

// x = tainted, y = specular;
varying vec2 intensity;

vec3 qz_aces_(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
vec4 qz_finish(vec4 c) {
    vec3 l = pow(max(c.rgb, vec3(0.0)), vec3(2.2)) * 1.30;
    return vec4(pow(qz_aces_(l), vec3(1.0 / 2.2)), c.a);
}

void main()
{
    gl_FragColor = qz_finish(vec4(vec3(intensity.y) + uniform_color.rgb * (intensity.x + emission_factor), uniform_color.a));
}
