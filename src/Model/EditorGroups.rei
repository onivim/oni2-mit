type t;

let create: unit => t;

let activeGroupId: t => int; // TODO: Should return an option, or be removed entirely
let getEditorGroupById: (t, int) => option(EditorGroup.t);
let getActiveEditorGroup: t => option(EditorGroup.t);

let isActive: (t, EditorGroup.t) => bool;
let isEmpty: (int, t) => bool;

let reduce: (t, Actions.t) => t;
