# TODO

Here I/we note the things that will/could be done in Upspring.

## @jochumdev (February 17, 2023)

### Soon/important

- fix .3do texture displaying.
- add support for directories as "archives".
- add a way to convert multiple .3do's into a single atlas.

### Maybe

- Extract object textures from an atlas.
- Combine object's again into an atlas.

### Unlikely
- Replace the abonded FLTK2 with GTK 3/4.


## Old entries by previous maintainers (cleaned up the stuff that is done)

### SpliFF (January 31, 2010)
- Show all supported types as default filetype in load/save selection.
- Try to preserve/scale normals and UV on scaling and rotation
- Fix any issues with objects "jumping around".
- Allow Lua metadata file to be exported without saving model
- Batch convert option for files, directories, archives (sdz/sd7) and maps
- Fix texture handling and display. Fix common problems automatically unless disabled in prefs.
- Create / replace parent object for all childs called 'base' at the top of the tree. You can rename this if you require.
- Fix not saving file extension

### OBJ
- Import/Export named objects. Hierarchies may still not be possible but your objects won't automatically merge either.

### 3DS
- Auto rotate on import unless UpSpring preferences say otherwise
- Maintain hierarchy if possible (may not be)

### Lua
* Look for <modelname>.lua in objects3d and import values and hierarchy if found
* Use Lua as 'god' for all values if that value is set.
* Disable any automatic rotations / fixes if Lua defines these.
* Option to raw import without Lua data (act like the Lua isn't there)
* Attempt to preserve Lua comments and source order/formatting.
* Allow seperate import of Lua (ie, to use a shared Lua or Lua stored outside objects3d/)


### Kloot (March 29, 2009)
- eliminate all compiler warnings

### Kloot (March 25, 2009)
- fix the scaling of normals on resize, etc.
