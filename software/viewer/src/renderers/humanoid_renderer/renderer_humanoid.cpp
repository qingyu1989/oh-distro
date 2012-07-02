// This renderer is a direct copy of the renderer in libbot2
// which makes up bot-rwx-viewer, but as it has no .h I copied it over here
// mfallon 25march2011

#include <iostream>

#include <lcm/lcm.h>
#include <lcm/lcm-cpp.hpp>
#include "lcmtypes/drc_lcmtypes.hpp"

#include <bot_vis/bot_vis.h>
#include <GL/gl.h>

#include "RobotStateListener.hpp"

#include <boost/function.hpp>

#define RENDERER_NAME "Humanoid"

typedef struct _RendererHumanoid 
{
  BotRenderer renderer;
  BotViewer          *viewer;
  BotGtkParamWidget *pw;
  boost::shared_ptr<fk::RobotStateListener> robotStateListener;
} RendererHumanoid;

static void
_renderer_free (BotRenderer *super)
{
  RendererHumanoid *self = (RendererHumanoid*) super->user;
  free(self);
}


static void 
_renderer_draw (BotViewer *viewer, BotRenderer *super)
{
    RendererHumanoid *self = (RendererHumanoid*) super->user;

    glEnable(GL_DEPTH_TEST);
    glPushMatrix();

    //do something

    glPopMatrix();
}

void 
setup_renderer_humanoid(BotViewer *viewer, int render_priority, lcm_t *lcm)
{
    RendererHumanoid *self = (RendererHumanoid*) calloc (1, sizeof (RendererHumanoid));
    lcm::LCM lcmCpp(lcm);
    self->robotStateListener = boost::shared_ptr<fk::RobotStateListener>(new fk::RobotStateListener(lcmCpp));

    BotRenderer *renderer = &self->renderer;

    renderer->draw = _renderer_draw;
    renderer->destroy = _renderer_free;

    renderer->widget = bot_gtk_param_widget_new();
    renderer->name = (char *) RENDERER_NAME;
    renderer->user = self;
    renderer->enabled = 1;

    self->viewer = viewer;

    self->pw = BOT_GTK_PARAM_WIDGET(renderer->widget);

    bot_viewer_add_renderer(viewer, &self->renderer, render_priority);


    //----------lcm stuff
}
