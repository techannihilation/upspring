//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
protected:
IEditor* callback;
fltk::Widget* multipleTypes;

void JointType(IKJointType jt);

public:
IK_UI(IEditor* callback);
~IK_UI();

void Show() const;
void Hide() { window->hide(); }
void AnimateToPos();
void Update();
