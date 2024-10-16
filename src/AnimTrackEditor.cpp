
//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorIncl.h"
#include "EditorDef.h"

#include "CfgParser.h"
#include "AnimationUI.h"

#include <set>
#include <iterator>

#include <fltk/draw.h>

AnimTrackEditorUI::AnimTrackEditorUI(IEditor* editor, TimelineUI* tl)
    : callback(editor), timeline(tl) {
  CreateUI();

  trackView->trackUI = this;
  browserList.ui = this;
  propBrowser->list(&browserList);
}

AnimTrackEditorUI::~AnimTrackEditorUI() { delete window; }

void AnimTrackEditorUI::Show() const {
  window->set_non_modal();
  window->show();
}

void AnimTrackEditorUI::Hide() const { window->hide(); }

void AnimTrackEditorUI::toggleMoveView() {}

void AnimTrackEditorUI::cmdAutoFitView() {
  float maxY = -100000.0F;
  float minY = -maxY;

  for (auto& object : objects) {
    for (auto& prop : object.props) {
      if (!prop.display) {
        continue;
      }

      float const step = (trackView->maxTime - trackView->minTime) / trackView->w();
      int x = 0;
      int lastkey = -1;
      for (float time = trackView->minTime; x < trackView->w(); time += step, x++) {
        float const y = prop.EvaluateY(time, lastkey);
        if (minY > y) {
          minY = y;
        }
        if (maxY < y) {
          maxY = y;
        }
      }
    }
  }
  float d = maxY - minY;
  if (d == 0.0F) {
    d = 1.0F;
  }
  d *= 0.1F;
  trackView->minY = minY - d;
  trackView->maxY = maxY + d;
  trackView->redraw();
}

void AnimTrackEditorUI::cmdAutoFitTime() {
  float minTime = 10000;
  float maxTime = -10000;
  bool keys = false;
  for (auto& object : objects) {
    for (auto& p : object.props) {
      if (!p.display) {
        continue;
      }

      AnimProperty* prop = p.prop;
      int const nk = prop->NumKeys();
      if (nk > 0) {
        keys = true;
        if (minTime > prop->GetKeyTime(0)) {
          minTime = prop->GetKeyTime(0);
        }
        if (maxTime < prop->GetKeyTime(nk - 1)) {
          maxTime = prop->GetKeyTime(nk - 1);
        }
      }
    }
  }
  if (keys) {
    float d = maxTime - minTime;
    if (d == 0.0F) {
      d = 1.0F;
    }
    d *= 0.1F;
    trackView->minTime = minTime - d;
    trackView->maxTime = maxTime + d;
    trackView->redraw();
  }
}

void AnimTrackEditorUI::cmdDeleteKeys() {}

void AnimTrackEditorUI::Update() {
  if (!window->visible()) {
    return;
  }

  UpdateBrowser();
  UpdateKeySel();
  window->redraw();
}

// update the property browser with a new list of properties
void AnimTrackEditorUI::UpdateBrowser() {
  Model* mdl = callback->GetMdl();

  if (chkLockObjects->value()) {
    std::vector<MdlObject*> obj = mdl->GetObjectList();
    // look for non-existant objects that are still in the browser
    auto ai = objects.begin();
    while (ai != objects.end()) {
      auto const i = ai++;
      if (find(obj.begin(), obj.end(), i->obj) == obj.end()) {
        objects.erase(i);
      }
    }
  } else {
    std::vector<MdlObject*> selObj = mdl->GetSelectedObjects();

    // look for objects in the view that shouldn't be there because of the new selection
    auto ai = objects.begin();
    while (ai != objects.end()) {
      auto const i = ai++;
      if (find(selObj.begin(), selObj.end(), i->obj) == selObj.end()) {
        objects.erase(i);
      }
    }

    // look for objects that aren't currently in the view and add them
    for (auto& i : selObj) {
      auto co = objects.begin();
      for (; co != objects.end(); ++co) {
        if (co->obj == i) {
          break;
        }
      }
      if (co == objects.end()) {
        AddObject(i);
      }
    }
  }
  propBrowser->layout();
}

void AnimTrackEditorUI::AddObject(MdlObject* o) {
  objects.emplace_back();
  objects.back().obj = o;

  // add properties from the object
  AnimationInfo const& ai = o->animInfo;
  for (const auto& propertie : ai.properties) {
    std::vector<Property>& props = objects.back().props;

    if (propertie->controller->GetNumMembers() > 0) {
      for (int a = 0; a < propertie->controller->GetNumMembers(); a++) {
        props.emplace_back();
        props.back().prop = propertie;
        props.back().display = false;
        props.back().member = a;
      }
    } else {
      props.emplace_back();
      props.back().prop = propertie;
      props.back().display = false;
      props.back().member = -1;
    }
  }
}

void AnimTrackEditorUI::UpdateKeySel() {
  for (auto& object : objects) {
    for (auto& prop : object.props) {
      Property* p = &prop;
      if (!p->display) {
        continue;
      }

      auto const nk = static_cast<size_t>(p->prop->NumKeys());
      if (nk != p->keySel.size()) {
        p->keySel.resize(nk);
        fill(p->keySel.begin(), p->keySel.end(), false);
        trackView->redraw();
      }
    }
  }
}
// ------------------------------------------------------------------------------------------------
// AnimObject::Property evaluation
// ------------------------------------------------------------------------------------------------

float AnimTrackEditorUI::Property::EvaluateY(float time, int& lastkey) const {
  static std::vector<char> valbuf;

  unsigned int const neededBufSize = prop->controller->GetSize();
  if (neededBufSize > valbuf.size()) {
    valbuf.resize(neededBufSize);
  }

  // get the right member out of the value
  std::pair<AnimController*, void*> memctl =
      prop->controller->GetMemberCtl(member, &valbuf.front());

  // only evaluate when it is actually usable
  if (memctl.first->CanConvertToFloat()) {
    prop->Evaluate(time, &valbuf.front(), &lastkey);
    return memctl.first->ToFloat(memctl.second);
  }

  return 0.0F;
}

// ------------------------------------------------------------------------------------------------
// List
// ------------------------------------------------------------------------------------------------

AnimTrackEditorUI::List::List() = default;

int AnimTrackEditorUI::List::children(const fltk::Menu* /*unused*/, const int* indexes, int level) {
  if (level == 0) {
    // object level
    return static_cast<int>(ui->objects.size());
  }
  if (level == 1) {
    // property level
    AnimObject const& obj = *element_at(ui->objects.begin(), ui->objects.end(), *indexes);
    return static_cast<int>(obj.props.size());
  }
  return -1;
}

fltk::Widget* AnimTrackEditorUI::List::child(const fltk::Menu* /*unused*/, const int* indexes,
                                             int level) {
  AnimObject& o = *element_at(ui->objects.begin(), ui->objects.end(), indexes[0]);
  item.textfont(fltk::HELVETICA);
  item.ui = ui;
  if (level == 0) {
    item.user_data(&o);
    item.label(o.obj->name.c_str());

    // set open/closed state
    item.set_flag(fltk::OPENED, o.open && !o.props.empty());

    // set selection
    item.set_flag(fltk::SELECTED, o.selected);
  } else if (level == 1) {
    Property& p = o.props[indexes[1]];
    item.user_data(&p);

    itemLabel = p.prop->name;
    if (p.member >= 0) {
      itemLabel += '.';
      itemLabel += p.prop->controller->GetMemberName(p.member);
    }
    item.label(itemLabel.c_str());
    if (p.display) {
      item.textfont(fltk::HELVETICA_BOLD);
    }

    // set selection
    if (p.selected) {
      item.set_flag(fltk::SELECTED);
    } else {
      item.clear_flag(fltk::SELECTED);
    }

    item.callback(item_callback);
  }
  item.w(0);
  return &item;
}

void AnimTrackEditorUI::List::flags_changed(const fltk::Menu* /*unused*/, fltk::Widget* w) {
  auto* bi = static_cast<BrowserItem*>(w->user_data());
  bi->open = ((w->flags() & fltk::OPENED) != 0);
  bi->selected = ((w->flags() & fltk::SELECTED) != 0);
}

void AnimTrackEditorUI::List::item_callback(fltk::Widget* w, void* user_data) {
  auto* bi = static_cast<BrowserItem*>(user_data);
  if (bi->IsProperty() && fltk::event_clicks() == 1) {
    auto* p = static_cast<Property*>(user_data);

    p->display = !p->display;
    (dynamic_cast<Item*>(w))->ui->UpdateKeySel();
    (dynamic_cast<Item*>(w))->ui->window->redraw();
  }
}

// -----------------------------------------------------------------------------
// Animation Track View - displays keyframes for the selected object properties
// -----------------------------------------------------------------------------

AnimKeyTrackView::AnimKeyTrackView(int X, int Y, int W, int H, const char* label)
    : Group(X, Y, W, H, label),
      minTime(0.0F),
      maxTime(5.0F),
      minY(0.0F),
      maxY(360.0F),
      curTime(0.0F),
      trackUI(nullptr) {
  movingView = movingKeys = zooming = false;
}

void AnimKeyTrackView::draw() {
  fltk::Group::draw();
  if (w() < 1) {
    return;
  }

  assert(trackUI);

  fltk::push_clip(Rectangle(w(), h()));

  fltk::setcolor(fltk::GRAY40);
  fltk::fillrect(fltk::Rectangle(w(), h()));

  char buffer[128];
  sprintf(buffer, "Time=[%3.2f,%3.2f], Y=[%4.4f,%4.4f]", minTime, maxTime, minY, maxY);
  fltk::setcolor(fltk::GRAY80);
  fltk::drawtext(buffer, 0.0F, 15.0F);

  curTime = trackUI->callback->GetTime();

  fltk::setcolor(fltk::WHITE);

  float const step = (maxTime - minTime) / w();
  for (auto& object : trackUI->objects) {
    for (auto& prop : object.props) {
      AnimTrackEditorUI::Property* p = &prop;
      if (!p->display) {
        continue;
      }

      int x = 1;
      int lastkey = -1;
      int key = -1;
      float const firstY = p->EvaluateY(minTime, lastkey);
      int prevY = h() * (1.0F - (firstY - minY) / (maxY - minY));

      for (float time = minTime + step; x < w(); time += step, x++) {
        float const y = p->EvaluateY(time, lastkey);
        int const cury = h() * (1.0F - (y - minY) / (maxY - minY));
        fltk::drawline(x, prevY, x + 1, cury);
        if (key != lastkey) {
          fltk::setcolor(p->keySel[lastkey] ? fltk::GREEN : fltk::BLUE);
          fltk::drawline(x + 1, cury - 10, x + 1, cury + 10);
          fltk::setcolor(fltk::WHITE);
          key = lastkey;
        }
        prevY = cury;
      }
    }
  }

  fltk::setcolor(fltk::YELLOW);
  int const x = (curTime - minTime) / step;
  fltk::drawline(x, 0, x, h());

  fltk::pop_clip();
}

void AnimKeyTrackView::SelectKeys(int mx, int my) {
  float const time = mx * (maxTime - minTime) / w() + minTime;  // time at selection pos
  float const y = my * (maxY - minY) / h() + minY;

  // Make sure key selection arrays have the correct size
  trackUI->UpdateKeySel();

  float const epsilon = 0.5F;

  for (auto& object : trackUI->objects) {
    for (auto& prop : object.props) {
      AnimTrackEditorUI::Property* p = &prop;
      if (!p->display) {
        continue;
      }

      for (int a = 0; a < p->prop->NumKeys(); a++) {
        float const kt = p->prop->GetKeyTime(a);
        if (kt < time + epsilon && kt > time - epsilon) {
          int lastkey = -1;
          float const ky = p->EvaluateY(kt, lastkey);
          if (ky < y + epsilon && ky > y - epsilon) {
            p->keySel[a] = true;
            redraw();
          }
        }
      }
    }
  }
}

int AnimKeyTrackView::handle(int event) {
  if (event == fltk::PUSH) {
    clickx = xpos = fltk::event_x();
    clicky = ypos = fltk::event_y();

    if (fltk::event_button() == 2) {
      movingView = true;
    }
    if (fltk::event_button() == 1) {
      movingKeys = true;
    }
    if (fltk::event_button() == 3) {
      zooming = true;
    }
    return 1;
  }
  if (event == fltk::LEAVE) {
    movingView = movingKeys = zooming = false;
  } else if (event == fltk::RELEASE) {
    if (fltk::event_x() == clickx && fltk::event_y() == clicky) {
      SelectKeys(clickx, clicky);
    }
    movingView = movingKeys = zooming = false;
  } else if (event == fltk::ENTER) {
    return 1;
  } else if (event == fltk::DRAG) {
    int const dx = fltk::event_x() - xpos;
    int const dy = fltk::event_y() - ypos;
    xpos = fltk::event_x();
    ypos = fltk::event_y();

    if (movingView) {
      float const ts = dx * (maxTime - minTime) / w();
      minTime -= ts;
      maxTime -= ts;
      float const ys = dy * (maxY - minY) / h();
      minY += ys;
      maxY += ys;
      redraw();
    }
    if (movingKeys) {
    }
    if (zooming) {
      float const xs = 1.0F + dx * 0.01F;
      float const ys = 1.0F - dy * 0.01F;

      float const midTime = (minTime + maxTime) * 0.5F;
      minTime = midTime + (minTime - midTime) * xs;
      maxTime = midTime + (maxTime - midTime) * xs;

      float const midY = (minY + maxY) * 0.5F;
      minY = midY + (minY - midY) * ys;
      maxY = midY + (maxY - midY) * ys;
      redraw();
    }
  }
  return fltk::Group::handle(event);
}
