#define SOKOL_IMPL

#ifdef _WIN32
#define SOKOL_D3D11
#else
#define SOKOL_GLCORE
#endif

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "shd.glsl.h"

#include <stdint.h>
#include <stdlib.h>

#define WIDTH 320
#define HEIGHT 240

static struct {
	struct {
		uint8_t data[WIDTH * HEIGHT];
		sg_image img;
		sg_image pal_img;
		sg_sampler smp;
	} fb;
	struct {
		sg_image img;
		sg_sampler smp;
		sg_buffer vbuf;
		sg_pipeline pip;
		sg_attachments attachments;
		sg_pass_action pass_action;
	} offscreen;
	struct {
		sg_buffer vbuf;
		sg_pipeline pip;
		sg_pass_action pass_action;
	} display;
} state;

static const uint32_t palette[16] = {
	0xFF000000,     // std black
	0xFFD70000,     // std blue
	0xFF0000D7,     // std red
	0xFFD700D7,     // std magenta
	0xFF00D700,     // std green
	0xFFD7D700,     // std cyan
	0xFF00D7D7,     // std yellow
	0xFFD7D7D7,     // std white
	0xFF000000,     // bright black
	0xFFFF0000,     // bright blue
	0xFF0000FF,     // bright red
	0xFFFF00FF,     // bright magenta
	0xFF00FF00,     // bright green
	0xFFFFFF00,     // bright cyan
	0xFF00FFFF,     // bright yellow
	0xFFFFFFFF,     // bright white
};

static const float verts[] = {
	0.0f, 0.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 1.0f,
	1.0f, 1.0f, 1.0f, 1.0f
};

static const float verts_flipped[] = {
	0.0f, 0.0f, 0.0f, 1.0f,
	1.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	1.0f, 1.0f, 1.0f, 0.0f
};

static void init(void) {
	sg_setup(&(sg_desc) {
		.environment = sglue_environment()
	});

	char title[256];
	snprintf(title, sizeof(title), "Emu v0.1.0 - %s", sg_query_backend() == SG_BACKEND_D3D11 ? "D3D11" : "GL");
	sapp_set_window_title(title);

	static uint32_t palette_buf[256];
	memcpy(palette_buf, palette, sizeof(palette));
	state.fb.pal_img = sg_make_image(&(sg_image_desc) {
		.width = 256,
		.height = 1,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.data = {
			.subimage[0][0] = {
				.ptr = palette_buf,
				.size = sizeof(palette_buf)
			}
		}
	});

	state.offscreen.pass_action = (sg_pass_action) {
		.colors[0] = { .load_action = SG_LOADACTION_DONTCARE }
	};
	state.offscreen.vbuf = sg_make_buffer(&(sg_buffer_desc) {
		.data = SG_RANGE(verts)
	});
	state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc) {
		.shader = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
		.layout = {
			.attrs = {
				[0].format = SG_VERTEXFORMAT_FLOAT2,
				[1].format = SG_VERTEXFORMAT_FLOAT2
			}
		},
		.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
		.depth.pixel_format = SG_PIXELFORMAT_NONE
	});

	state.display.pass_action = (sg_pass_action) {
		.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.05f, 0.05f, 0.05f, 1.0f } }
	};
	state.display.vbuf = sg_make_buffer(&(sg_buffer_desc) {
		.data = {
			.ptr = sg_query_features().origin_top_left ? verts : verts_flipped,
			.size = sizeof(verts)
		}
	});
	state.display.pip = sg_make_pipeline(&(sg_pipeline_desc) {
		.shader = sg_make_shader(display_shader_desc(sg_query_backend())),
		.layout = {
			.attrs = {
				[0].format = SG_VERTEXFORMAT_FLOAT2,
				[1].format = SG_VERTEXFORMAT_FLOAT2
			}
		},
		.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
	});

	state.fb.img = sg_make_image(&(sg_image_desc) {
		.width = WIDTH,
		.height = HEIGHT,
		.pixel_format = SG_PIXELFORMAT_R8,
		.usage = SG_USAGE_STREAM
	});
	state.fb.smp = sg_make_sampler(&(sg_sampler_desc) {
		.min_filter = SG_FILTER_NEAREST,
		.mag_filter = SG_FILTER_NEAREST,
		.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_v = SG_WRAP_CLAMP_TO_EDGE
	});

	state.offscreen.img = sg_make_image(&(sg_image_desc) {
		.render_target = true,
		.width = 2 * WIDTH,
		.height = 2 * HEIGHT,
		.sample_count = 1,
	});
	state.offscreen.smp = sg_make_sampler(&(sg_sampler_desc) {
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
	});
	state.offscreen.attachments = sg_make_attachments(&(sg_attachments_desc) {
		.colors[0].image = state.offscreen.img,
	});

	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		state.fb.data[i] = i % 256;
	}
}

static void apply_viewport(void) {
	float cw = sapp_widthf();
	if (cw < 1.0f) {
		cw = 1.0f;
	}
	float ch = sapp_heightf();
	if (ch < 1.0f) {
		ch = 1.0f;
	}

	const float canvas_aspect = cw / ch;
	const float emu_aspect = (float) WIDTH / (float) HEIGHT;

	float vp_x, vp_y, vp_w, vp_h;
	if (emu_aspect < canvas_aspect) {
		vp_y = 0.0f;
		vp_h = ch;
		vp_w = ch * emu_aspect;
		vp_x = (cw - vp_w) / 2;
	} else {
		vp_x = 0.0f;
		vp_w = cw;
		vp_h = cw / emu_aspect;
		vp_y = (ch - vp_h) / 2;
	}
	sg_apply_viewportf(vp_x, vp_y, vp_w, vp_h, true);
}

static void frame(void) {
	sg_update_image(state.fb.img, &(sg_image_data) {
		.subimage[0][0] = {
			.ptr = state.fb.data,
			.size = sizeof(state.fb.data)
		}
	});

	sg_begin_pass(&(sg_pass) {
		.action = state.offscreen.pass_action,
		.attachments = state.offscreen.attachments
	});
	sg_apply_pipeline(state.offscreen.pip);
	sg_apply_bindings(&(sg_bindings) {
		.vertex_buffers[0] = state.offscreen.vbuf,
		.images = {
			[IMG_fb_tex] = state.fb.img,
			[IMG_pal_tex] = state.fb.pal_img
		},
		.samplers[SMP_smp] = state.fb.smp
	});
	const offscreen_vs_params_t vs_params = {
		.uv_offset = { 0, 0 },
		.uv_scale = { 1, 1 }
	};
	sg_apply_uniforms(UB_offscreen_vs_params, &SG_RANGE(vs_params));
	sg_draw(0, 4, 1);
	sg_end_pass();

	sg_begin_pass(&(sg_pass) {
		.action = state.display.pass_action,
		.swapchain = sglue_swapchain()
	});
	apply_viewport();
	sg_apply_pipeline(state.display.pip);
	sg_apply_bindings(&(sg_bindings) {
		.vertex_buffers[0] = state.display.vbuf,
		.images[IMG_tex] = state.offscreen.img,
		.samplers[SMP_smp] = state.offscreen.smp
	});
	sg_draw(0, 4, 1);
	sg_apply_viewport(0, 0, sapp_width(), sapp_height(), true);
	sg_end_pass();
	sg_commit();
}

static void event(const sapp_event *event) {
	if (event->type == SAPP_EVENTTYPE_KEY_DOWN && event->key_code == SAPP_KEYCODE_ENTER && event->modifiers == SAPP_MODIFIER_ALT) {
		sapp_toggle_fullscreen();
	}
}

static void cleanup(void) {
	sg_shutdown();
}

sapp_desc sokol_main(int argc, char **argv) {
	return (sapp_desc) {
		.init_cb = init,
		.frame_cb = frame,
		.event_cb = event,
		.cleanup_cb = cleanup,
		.width = 2 * WIDTH,
		.height = 2 * HEIGHT,
		.window_title = "Emu v0.1.0"
	};
}
