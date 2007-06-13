#ifndef __NDS_UI_H__
#define __NDS_UI_H__

struct ndsui_component {
	int x, y;  /* top-left */
	int w, h;  /* in pixels */
	int visible;
	void (*show)(struct ndsui_component *self);
	void (*resize)(struct ndsui_component *self, int w, int h);
	void (*pen_down)(struct ndsui_component *self, int x, int y);
	void (*pen_move)(struct ndsui_component *self, int x, int y);
	void (*pen_up)(struct ndsui_component *self);
	void (*destroy)(struct ndsui_component *self);
	void *data;
	struct ndsui_component *next;
};

struct ndsui_component *ndsui_new_component(int x, int y, int w, int h);
void ndsui_show_component(struct ndsui_component *c);
void ndsui_hide_component(struct ndsui_component *c);
void ndsui_draw_component(struct ndsui_component *c);
void ndsui_undraw_component(struct ndsui_component *c);
void ndsui_resize_component(struct ndsui_component *c, int w, int h);

#endif  /* __NDS_UI_H__ */
