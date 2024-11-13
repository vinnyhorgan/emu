#define SOKOL_IMPL
#define SOKOL_D3D11
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "shd.glsl.h"

#include <stdint.h>
#include <stdlib.h>

#define WIDTH 320
#define HEIGHT 240

static struct {
	uint32_t *fb;
	sg_image fb_img;
	sg_sampler fb_smp;
	sg_pass_action pass_action;
	sg_buffer vbuf;
	sg_pipeline pip;
} state;

static const float verts[] = {
	0.0f, 0.0f, 0.0f, 1.0f,
	1.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	1.0f, 1.0f, 1.0f, 0.0f
};

static void init(void) {
	state.fb = calloc(1, WIDTH * HEIGHT * sizeof(uint32_t));
	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		state.fb[i] = 0xFFFFFFFF;
	}
	state.fb[10 + 10 * WIDTH] = 0xFF0000FF;

	sg_setup(&(sg_desc) {
		.environment = sglue_environment()
	});

	state.pass_action = (sg_pass_action) {
		.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.05f, 0.05f, 0.05f, 1.0f } }
	};

	state.fb_img = sg_make_image(&(sg_image_desc) {
		.width = WIDTH,
		.height = HEIGHT,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.usage = SG_USAGE_STREAM
	});

	state.fb_smp = sg_make_sampler(&(sg_sampler_desc) {
		.min_filter = SG_FILTER_NEAREST,
		.mag_filter = SG_FILTER_NEAREST,
		.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_v = SG_WRAP_CLAMP_TO_EDGE
	});

	state.vbuf = sg_make_buffer(&(sg_buffer_desc) {
		.data = {
			.ptr = verts,
			.size = sizeof(verts)
		}
	});

	state.pip = sg_make_pipeline(&(sg_pipeline_desc) {
		.shader = sg_make_shader(shd_shader_desc(sg_query_backend())),
		.layout = {
			.attrs = {
				[0].format = SG_VERTEXFORMAT_FLOAT2,
				[1].format = SG_VERTEXFORMAT_FLOAT2
			}
		},
		.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
	});
}

static void calc_viewport(int *x, int *y, int *w, int *h) {
	int vp_x = 0, vp_y = 0, vp_w = sapp_width(), vp_h = sapp_height();

	if (vp_w * HEIGHT < vp_h * WIDTH) {
		int discrete_width = vp_w - vp_w % WIDTH;
		int discrete_height = HEIGHT * discrete_width / WIDTH;

		vp_x = (vp_w - discrete_width) / 2;
		vp_y = (vp_h - discrete_height) / 2;

		vp_w = discrete_width;
		vp_h = discrete_height;
	} else {
		int discrete_height = vp_h - vp_h % HEIGHT;
		int discrete_width = WIDTH * discrete_height / HEIGHT;

		vp_x = (vp_w - discrete_width) / 2;
		vp_y = (vp_h - discrete_height) / 2;

		vp_w = discrete_width;
		vp_h = discrete_height;
	}

	*x = vp_x;
	*y = vp_y;
	*w = vp_w;
	*h = vp_h;
}

static void frame(void) {
	sg_update_image(state.fb_img, &(sg_image_data) {
		.subimage[0][0] = {
			.ptr = state.fb,
			.size = WIDTH * HEIGHT * sizeof(uint32_t)
		}
	});

	sg_begin_pass(&(sg_pass) {
		.action = state.pass_action,
		.swapchain = sglue_swapchain()
	});

	int vp_x, vp_y, vp_w, vp_h;
	calc_viewport(&vp_x, &vp_y, &vp_w, &vp_h);
	sg_apply_viewport(vp_x, vp_y, vp_w, vp_h, true);

	sg_apply_pipeline(state.pip);
	sg_apply_bindings(&(sg_bindings) {
		.vertex_buffers[0] = state.vbuf,
		.images[IMG_tex] = state.fb_img,
		.samplers[SMP_smp] = state.fb_smp
	});
	sg_draw(0, 4, 1);
	sg_end_pass();
	sg_commit();
}

static void cleanup(void) {
	sg_shutdown();
}

sapp_desc sokol_main(int argc, char **argv) {
	return (sapp_desc) {
		.init_cb = init,
		.frame_cb = frame,
		.cleanup_cb = cleanup,
		.width = WIDTH * 3,
		.height = HEIGHT * 3,
		.window_title = "Emu v0.1.0"
	};
}
