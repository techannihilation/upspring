//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
protected:
IEditor* callback;

public:
MappingUI(IEditor* callback);
~MappingUI();

void Show() const;
void Hide() { window->hide(); }
void flipUVs();
void mirrorUVs();
