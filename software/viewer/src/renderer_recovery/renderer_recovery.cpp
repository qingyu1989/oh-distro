// Renderer for a point-and-click message publisher
// used to send a message to relocalize a robot
// this was orginally part of envoy/renderers
// mfallon aug2011
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <math.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <bot_vis/bot_vis.h>
#include <bot_core/bot_core.h>


#include <vector>

#include <lcmtypes/drc_lcmtypes.h>
//#include <lcmtypes/drc_lcmtypes.hpp>
#include <lcmtypes/bot_core.h>

#include <maps/ViewClient.hpp>
#include <maps/BotWrapper.hpp>

#define RENDERER_NAME "Recovery"
#define PARAM_SEND_RECOVERY "Send Command"
#define PARAM_RECOVERY_MODE "Mode"


typedef enum _recovery_mode_t {
    MODE_PROJECTILE, MODE_UP_TO_DOWN, MODE_FLAT_OUT, MODE_KNEE_SET, MODE_KNEE_RISE, MODE_KNEE_FINISH
} recovery_mode_t;


////////////////////////////// END OF CODE COPIED IN FROM COMMON_UTILS
typedef struct _RendererRecovery {
  BotRenderer renderer;
  BotEventHandler ehandler;
  BotViewer *viewer;
  lcm_t *lc;
  BotGtkParamWidget *pw;
  
  int mode;
}RendererRecovery;

static void
_draw (BotViewer *viewer, BotRenderer *renderer)
{
  RendererRecovery *self = (RendererRecovery*) renderer;
}

static int 
mouse_press (BotViewer *viewer, BotEventHandler *ehandler, const double ray_start[3], 
    const double ray_dir[3], const GdkEventButton *event)
{
  RendererRecovery *self = (RendererRecovery*) ehandler->user;
  return 0;
}

static int mouse_release(BotViewer *viewer, BotEventHandler *ehandler,
    const double ray_start[3], const double ray_dir[3],
    const GdkEventButton *event)
{
  RendererRecovery *self = (RendererRecovery*) ehandler->user;
  return 0;
}

static int mouse_motion (BotViewer *viewer, BotEventHandler *ehandler,
    const double ray_start[3], const double ray_dir[3],
    const GdkEventMotion *event)
{
  RendererRecovery *self = (RendererRecovery*) ehandler->user;
  return 0;
}

static int key_press (BotViewer *viewer, BotEventHandler *ehandler, 
    const GdkEventKey *event)
{
  RendererRecovery *self = (RendererRecovery*) ehandler->user;
  return 0;
}

static void on_param_widget_changed(BotGtkParamWidget *pw, const char *name, void *user)
{
  RendererRecovery *self = (RendererRecovery*) user;
  
  self->mode = (int) bot_gtk_param_widget_get_enum(self->pw, PARAM_RECOVERY_MODE);
  std::cout << "Recovery Mode: "<<self->mode <<"\n";

  if(!strcmp(name, PARAM_SEND_RECOVERY)) {
    fprintf(stderr,"\nSending Recovery\n");
    drc_recovery_t msg;
    msg.mode = (int8_t) self->mode;
    drc_recovery_t_publish(self->lc, "RECOVERY_CMD", &msg);
    
  }
}

static void
_free (BotRenderer *renderer)
{
  RendererRecovery *self = (RendererRecovery*) renderer;
  free (renderer);
}

BotRenderer *renderer_recovery_new (BotViewer *viewer, int render_priority, lcm_t *lcm, BotParam * param, BotFrames * frames)
{
  RendererRecovery *self = (RendererRecovery*) calloc (1, sizeof (RendererRecovery));
  self->viewer = viewer;
  self->renderer.draw = _draw;
  self->renderer.destroy = _free;
  self->renderer.name = RENDERER_NAME;
  self->renderer.user = self;
  self->renderer.enabled = 1;

  BotEventHandler *ehandler = &self->ehandler;
  ehandler->name = (char*) RENDERER_NAME;
  ehandler->enabled = 0;
  ehandler->pick_query = NULL;
  ehandler->key_press = NULL;
  ehandler->hover_query = NULL;
  ehandler->mouse_press = NULL;
  ehandler->mouse_release = NULL;
  ehandler->mouse_motion = NULL;
  ehandler->user = self;

  bot_viewer_add_event_handler(viewer, &self->ehandler, render_priority);

  self->lc = lcm; //globals_get_lcm_full(NULL,1);

  self->pw = BOT_GTK_PARAM_WIDGET(bot_gtk_param_widget_new());
  bot_gtk_param_widget_add_enum(self->pw, PARAM_RECOVERY_MODE, BOT_GTK_PARAM_WIDGET_MENU, 0, "Projectile", MODE_PROJECTILE, 
                                "Up-to-Down", MODE_UP_TO_DOWN, "Flat Out", MODE_FLAT_OUT, "Knee Set", MODE_KNEE_SET,
                                "Knee Rise", MODE_KNEE_RISE, "Finish", MODE_KNEE_FINISH, NULL);
  bot_gtk_param_widget_add_buttons(self->pw, PARAM_SEND_RECOVERY, NULL);
  
  g_signal_connect(G_OBJECT(self->pw), "changed", G_CALLBACK(on_param_widget_changed), self);
  self->renderer.widget = GTK_WIDGET(self->pw);

  return &self->renderer;
}

void setup_renderer_recovery(BotViewer *viewer, int render_priority, lcm_t *lcm, BotParam * param,
    BotFrames * frames)
{
  bot_viewer_add_renderer_on_side(viewer, renderer_recovery_new(viewer, render_priority, lcm, param, frames),
      render_priority , 0);
}
