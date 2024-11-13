@vs vs
layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

out vec2 uv;

void main() {
	gl_Position = vec4(in_pos * 2.0 - 1.0, 0.5, 1.0);
	uv = in_uv;
}
@end

@fs fs
layout(binding = 0) uniform texture2D tex;
layout(binding = 0) uniform sampler smp;
in vec2 uv;

out vec4 frag_color;

void main() {
	frag_color = vec4(texture(sampler2D(tex, smp), uv).xyz, 1.0);
}
@end

@program shd vs fs
