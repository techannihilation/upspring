//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorIncl.h"
#include "EditorDef.h"

#include "Util.h"

#include "Animation.h"

#include <utility>

//-----------------------------------------------------------------------
// Animation controllers for Quaternion,Vector3 and float
//-----------------------------------------------------------------------

class FloatController : public AnimController {
 public:
  virtual ~FloatController() = default;
  int GetSize() override { return sizeof(float); }
  void LinearInterp(void* a, void* b, float x, void* out) override {
    *static_cast<float*>(out) = *static_cast<float*>(a) * (1.0F - x) + *static_cast<float*>(b) * x;
  }
  void Copy(void* src, void* dst) override {
    *static_cast<float*>(dst) = *static_cast<float*>(src);
  }
  bool CanConvertToFloat() override { return true; }
  float ToFloat(void* v) override { return *static_cast<float*>(v); }
  AnimKeyType GetType() override { return ANIMKEY_Float; }
};

AnimController* AnimController::GetFloatController() {
  static FloatController fc;
  return &fc;
}

class QuatController : public AnimController {
 public:
  virtual ~QuatController() = default;
  int GetSize() override { return sizeof(Quaternion); }
  void LinearInterp(void* a, void* b, float x, void* out) override {
    auto* qa = static_cast<Quaternion*>(a);
    auto* qb = static_cast<Quaternion*>(b);

    qa->slerp(qb, x, static_cast<Quaternion*>(out), Quaternion::qshort);
  }
  void Copy(void* src, void* dst) override {
    *static_cast<Quaternion*>(dst) = *static_cast<Quaternion*>(src);
  }

  // Fake euler angles
  template <int Axis>
  class QuatToEulerController : public FloatController {
   public:
    ~QuatToEulerController() override = default;
    float ToFloat(void* v) override {
      auto* q = static_cast<Quaternion*>(v);
      Matrix m;
      q->makematrix(&m);
      Vector3 const euler = m.calcEulerYXZ();
      return euler.v[Axis];
    }
  };

  int GetNumMembers() override { return 3; }
  std::pair<AnimController*, void*> GetMemberCtl(int m, void* inst) override {
    static QuatToEulerController<0> q2e0;
    static QuatToEulerController<1> q2e1;
    static QuatToEulerController<2> q2e2;
    AnimController* actl[] = {&q2e0, &q2e1, &q2e2};

    return std::pair<AnimController*, void*>(actl[m], inst);
  }
  const char* GetMemberName(int m) override {
    const char* axis[] = {"x", "y", "z"};
    return axis[m];
  }
  AnimKeyType GetType() override { return ANIMKEY_Quat; }
};

AnimController* AnimController::GetQuaternionController() {
  static QuatController qc;
  return &qc;
}

class EulerAngleController : public FloatController {
 public:
  ~EulerAngleController() override = default;
  void LinearInterp(void* A, void* B, float x, void* out) override {
    float& a = *static_cast<float*>(A);
    float& b = *static_cast<float*>(B);
    float& o = *static_cast<float*>(out);

    if (a > 2 * M_PI) {
      a -= 2 * M_PI;
    }
    if (a < 0.0F) {
      a += 2 * M_PI;
    }
    if (b > 2 * M_PI) {
      b -= 2 * M_PI;
    }
    if (b < 0.0F) {
      b += 2 * M_PI;
    }

    float v = b - a;
    if (fabsf(v) > M_PI) {
      if (v > 0) {
        v -= 2 * M_PI;
      } else {
        v += 2 * M_PI;
      }
    }

    o = a + v * x;
  }
};

AnimController* AnimController::GetEulerAngleController() {
  static EulerAngleController eac;
  return &eac;
}

//-----------------------------------------------------------------------
// AnimProperty - holds animation info for an object property
//-----------------------------------------------------------------------

AnimProperty::AnimProperty(AnimController* ctl, std::string name, int offset)
    : offset(offset),
      elemSize(sizeof(float) + controller->GetSize()),
      name(std::move(name)),
      controller(ctl) {}

AnimProperty::AnimProperty() = default;

AnimProperty::~AnimProperty() = default;

int AnimProperty::GetKeyIndex(float time, const int* lastkey) {
  int const nk = NumKeys();
  int a = 0;
  if ((lastkey != nullptr) && *lastkey >= 0) {
    a = *lastkey;
  }
  for (; a < nk; a++) {
    if (GetKeyTime(a) > time) {
      return a - 1;
    }
  }
  return NumKeys() - 1;
}

void AnimProperty::Evaluate(float time, void* value, int* lastkey) {
  if (keyData.empty()) {
    return;
  }

  int const index = GetKeyIndex(time, lastkey);
  if (lastkey != nullptr) {
    *lastkey = index;
  }

  if (index < 0) {
    controller->Copy(GetKeyData(0), value);
  } else if (index + 1 < NumKeys()) {
    float const timeA = GetKeyTime(index);
    float const timeB = GetKeyTime(index + 1);

    float const x = (time - timeA) / (timeB - timeA);
    controller->LinearInterp(GetKeyData(index), GetKeyData(index + 1), x, value);
  } else {
    // return the value of a single key
    controller->Copy(GetKeyData(index), value);
  }
}

void AnimProperty::InsertKey(void* data, float time) {
  int const index = GetKeyIndex(time);

  // create a new key or modify an existing one?
  if (keyData.empty() || GetKeyTime(index) <= time - EPSILON ||
      GetKeyTime(index) >= time + EPSILON) {
    assert(!keyData.empty() || index == -1);
    keyData.insert(keyData.begin() + elemSize * (index + 1), elemSize, 0);

    // set keyframe
    GetKeyTime(index + 1) = time;
    controller->Copy(data, GetKeyData(index + 1));
  } else {
    controller->Copy(data, GetKeyData(index));
  }
}

void AnimProperty::ChopAnimation(float endTime) {
  for (int k = 0; k < NumKeys(); k++) {
    if (GetKeyTime(k) > endTime) {
      keyData.erase(keyData.begin() + elemSize * k, keyData.end());
      break;
    }
  }
}

AnimProperty* AnimProperty::Clone() const {
  auto* cp = new AnimProperty();

  cp->keyData = keyData;
  cp->controller = controller;
  cp->name = name;
  cp->offset = offset;
  cp->elemSize = elemSize;

  return cp;
}

//-----------------------------------------------------------------------
// AnimationInfo
//-----------------------------------------------------------------------

AnimationInfo::~AnimationInfo() {
  for (auto& propertie : properties) {
    delete propertie;
  }
}

void AnimationInfo::AddProperty(AnimController* ctl, const char* name, int offset) {
  properties.push_back(new AnimProperty(ctl, name, offset));
}

void AnimationInfo::InsertKeyFrames(void* obj, float time) {
  for (auto* pi : properties) {
    bool edited = false;
    int const size = pi->controller->GetSize();
    char* currentValue = (static_cast<char*>(obj)) + pi->offset;

    if (pi->keyData.empty() || pi->GetKeyTime(0) > time ||
        pi->GetKeyTime(pi->NumKeys() - 1) < time) {
      edited = true;
    } else {
      char* value = new char[size];

      pi->Evaluate(time, value);
      if (memcmp(value, currentValue, size) != 0) {
        // value is not the same as calculated value, so it is edited and a new key
        // should be added for it
        edited = true;
      }

      delete[] value;
    }

    if (edited) {
      pi->InsertKey(currentValue, time);
    }
  }
}

void AnimationInfo::Evaluate(void* obj, float time) {
  for (auto& propertie : properties) {
    propertie->Evaluate(time, static_cast<char*>(obj) + propertie->offset);
  }
}

void AnimationInfo::ClearAnimData() {
  for (auto& propertie : properties) {
    propertie->Clear();
  }
}

void AnimationInfo::CopyTo(AnimationInfo& animInfo) {
  // free anim data of the destination object
  for (auto& propertie : animInfo.properties) {
    delete propertie;
  }

  animInfo.properties.resize(properties.size());
  for (uint i = 0; i < properties.size(); i++) {
    animInfo.properties[i] = properties[i]->Clone();
  }
}
