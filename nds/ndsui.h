#ifndef XROAR_NDS_NDSUI_H_
#define XROAR_NDS_NDSUI_H_

struct ndsui_component {
	unsigned int id;  /* incrementing - not used internally */
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

void ndsui_add_component(struct ndsui_component *c);
void ndsui_show_all_components(void);
void ndsui_clear_component_list(void);

void ndsui_pen_down(int x, int y);
void ndsui_pen_move(int x, int y);
void ndsui_pen_up(void);

struct ndsui_component *ndsui_new_component(int x, int y, int w, int h);
void ndsui_show_component(struct ndsui_component *c);
void ndsui_hide_component(struct ndsui_component *c);
void ndsui_draw_component(struct ndsui_component *c);
void ndsui_undraw_component(struct ndsui_component *c);
void ndsui_resize_component(struct ndsui_component *c, int w, int h);

#endif  /* XROAR_NDS_NDSUI_H_ */
