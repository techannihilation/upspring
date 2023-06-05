
//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include <fltk/run.h>

#include "AnimationUI.h"
#include "CfgParser.h"
#include "EditorDef.h"
#include "EditorIncl.h"

#ifdef WIN32
#include <windows.h>

class Timer {
  LARGE_INTEGER startCounter;

 public:
  void Reset() { QueryPerformanceCounter(&startCounter); }

  unsigned int GetTicks(void) {
    LARGE_INTEGER cur;
    QueryPerformanceCounter(&cur);

    cur.QuadPart -= startCounter.QuadPart;
    cur.QuadPart *= 1000;

    LARGE_INTEGER tickps;
    QueryPerformanceFrequency(&tickps);
    cur.QuadPart /= tickps.QuadPart;

    return (DWORD)cur.QuadPart;
  }
};

#else
// #error implement tick counting stuff for other OSes here
// use a stub Timer replacement to at least allow compiling
class Timer {
  unsigned int startCounter_;

 public:
  void Reset() {}
  static unsigned int GetTicks() { return 0; }
};
#endif

TimelineUI::TimelineUI(IEditor* editor)
    : callback(editor), timer(new Timer), time(0.0F), isPlaying(false) {
  CreateUI();

  timeSlider->step(0.01F);
}

TimelineUI::~TimelineUI() {
  delete window;
  delete timer;
}

void TimelineUI::Show() const {
  window->set_non_modal();
  window->show();
}

void TimelineUI::SliderCallback() {
  if (!isPlaying) {
    time = timeSlider->value();

    Model* mdl = callback->GetMdl();
    if (mdl->root != nullptr) {
      mdl->root->UpdateAnimation(time);
    }
    callback->RedrawViews();
  }
}

void TimelineUI::idle_cb(void* arg) {
  auto* ui = static_cast<TimelineUI*>(arg);

  // update time and calculated animated state
  unsigned int const ticks = Timer::GetTicks();
  float const dt = (ticks - ui->prevTicks) * 0.001F;
  ui->prevTicks = ticks;

  ui->time += dt;
  ui->timeSlider->value(ui->time);

  if (ui->time > ui->timeSlider->maximum()) {
    ui->time = 0;
  }

  Model* mdl = ui->callback->GetMdl();
  if (mdl->root != nullptr) {
    mdl->root->UpdateAnimation(ui->time);
  }

  // update views
  ui->callback->RedrawViews();
  ui->timeSlider->redraw();
}

void TimelineUI::cmdPlayStop() {
  if (isPlaying) {
    Stop();
  } else {
    Play();
    time = 0.0F;
  }
}

void TimelineUI::Stop() {
  isPlaying = false;

  fltk::remove_idle(idle_cb, this);
  playstopButton->label("Play");
  playstopButton->redraw();
}

void TimelineUI::Play() {
  isPlaying = true;
  playstopButton->label("Stop");

  fltk::add_idle(idle_cb, this);

  timer->Reset();
  prevTicks = 0;
  playstopButton->redraw();
}

void TimelineUI::Hide() {
  if (isPlaying) {
    cmdPlayStop();
  }

  window->hide();
}

void TimelineUI::cmdPause() {
  if (isPlaying) {
    Stop();
  } else {
    Play();
  }
}

static void ChopAnimationInfo(MdlObject* o, float time) {
  for (auto& propertie : o->animInfo.properties) {
    propertie->ChopAnimation(time);
  }

  for (auto& child : o->childs) {
    ChopAnimationInfo(child, time);
  }
}

void TimelineUI::cmdSetLength() {
  char ls[32];
  sprintf(ls, "%g", timeSlider->maximum());
  const char* r = fltk::input("Enter length of animation", ls);
  if (r != nullptr) {
    float const m = atof(r);
    if (m > 0.01F) {
      Model* mdl = callback->GetMdl();
      if (mdl->root != nullptr) {
        ChopAnimationInfo(mdl->root, m);
      }

      timeSlider->maximum(m);
      window->redraw();
    }
  }
}

void TimelineUI::Update() {
  if (autoKeying->value()) {
    InsertKeys(true);
  }
}

static void InsertKeysHelper(MdlObject* o, float time) {
  o->animInfo.InsertKeyFrames(o, time);
  for (auto& child : o->childs) {
    InsertKeysHelper(child, time);
  }
}

void TimelineUI::InsertKeys(bool autoInsert) {
  Model* mdl = callback->GetMdl();

  time = timeSlider->value();

  if (mdl->root != nullptr) {
    InsertKeysHelper(mdl->root, time);

    if (!autoInsert) {
      callback->Update();
    }
  }
}
