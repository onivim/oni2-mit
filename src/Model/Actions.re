/*
 * Actions.re
 *
 * Encapsulates actions that can impact the editor state
 */

open EditorCoreTypes;
open Oni_Core;
open Oni_Input;
open Oni_Syntax;

module Ext = Oni_Extensions;

[@deriving show({with_path: false})]
type t =
  | Init
  | Tick(tick)
  | ActivityBar(ActivityBar.action)
  | BufferHighlights(BufferHighlights.action)
  | BufferDisableSyntaxHighlighting(int)
  | BufferEnter([@opaque] Vim.BufferMetadata.t, option(string))
  | BufferUpdate([@opaque] BufferUpdate.t)
  | BufferRenderer(BufferRenderer.action)
  | BufferSaved([@opaque] Vim.BufferMetadata.t)
  | BufferSetIndentation(int, [@opaque] IndentationSettings.t)
  | BufferSetModified(int, bool)
  | BufferSyntaxHighlights([@opaque] list(Protocol.TokenUpdate.t))
  | SyntaxServerClosed
  | Command(string)
  | CommandsRegister(list(command))
  // Execute a contribute command, from an extension
  | CommandExecuteContributed(string)
  | CompletionAddItems(
      [@opaque] CompletionMeet.t,
      [@opaque] list(CompletionItem.t),
    )
  | ConfigurationReload
  | ConfigurationSet([@opaque] Configuration.t)
  // ConfigurationTransform(fileName, f) where [f] is a configurationTransformer
  // opens the file [fileName] and applies [f] to the loaded JSON.
  | ConfigurationTransform(string, configurationTransformer)
  | DarkModeSet(bool)
  | DefinitionAvailable(
      int,
      Location.t,
      [@opaque] LanguageFeatures.DefinitionResult.t,
    )
  | Extension(Extensions.action)
  | References(References.actions)
  | KeyBindingsSet([@opaque] Keybindings.t)
  // Reload keybindings from configuration
  | KeyBindingsReload
  | KeyDown([@opaque] Revery.Key.KeyEvent.t)
  | KeyUp([@opaque] Revery.Key.KeyEvent.t)
  | TextInput([@opaque] Revery.Events.textInputEvent)
  | HoverShow
  | ChangeMode([@opaque] Vim.Mode.t)
  | DiagnosticsHotKey
  | DiagnosticsSet(Uri.t, string, [@opaque] list(Diagnostic.t))
  | DiagnosticsClear(string)
  | SelectionChanged([@opaque] VisualRange.t)
  // LoadEditorFont is the request to load a new font
  // If successful, a SetEditorFont action will be dispatched.
  | LoadEditorFont(string, int)
  | SetEditorFont([@opaque] EditorFont.t)
  | RecalculateEditorView([@opaque] option(Buffer.t))
  | NotifyKeyPressed(float, string)
  | DisableKeyDisplayer
  | EnableKeyDisplayer
  | KeyboardInput(string)
  | WindowSetActive(int, int)
  | WindowTitleSet(string)
  | WindowTreeSetSize(int, int)
  | EditorGroupAdd(editorGroup)
  | EditorGroupSetSize(int, EditorSize.t)
  | EditorCursorMove(EditorId.t, [@opaque] list(Vim.Cursor.t))
  | EditorSetScroll(EditorId.t, float)
  | EditorScroll(EditorId.t, float)
  | EditorScrollToLine(EditorId.t, int)
  | EditorScrollToColumn(EditorId.t, int)
  | ShowNotification(Notification.t)
  | HideNotification(Notification.t)
  | FileExplorer(FileExplorer.action)
  | LanguageFeature(LanguageFeatures.action)
  | QuickmenuShow(quickmenuVariant)
  | QuickmenuInput(string)
  | QuickmenuInputClicked(int)
  | QuickmenuCommandlineUpdated(string, int)
  | QuickmenuUpdateRipgrepProgress(progress)
  | QuickmenuUpdateFilterProgress([@opaque] array(menuItem), progress)
  | QuickmenuSearch(string)
  | QuickmenuClose
  | ListFocus(int)
  | ListFocusUp
  | ListFocusDown
  | ListSelect
  | ListSelectBackground
  | OpenFileByPath(string, option(WindowTree.direction), option(Location.t))
  | AddSplit(WindowTree.direction, WindowTree.split)
  | RemoveSplit(int)
  | OpenConfigFile(string)
  | QuitBuffer([@opaque] Vim.Buffer.t, bool)
  | Quit(bool)
  | RegisterQuitCleanup(unit => unit)
  | SearchClearMatchingPair(int)
  | SearchSetMatchingPair(int, Location.t, Location.t)
  | SearchSetHighlights(int, list(Range.t))
  | SearchClearHighlights(int)
  | SetLanguageInfo([@opaque] Ext.LanguageInfo.t)
  | ThemeLoadByPath(string, string)
  | ThemeLoadByName(string)
  | SetIconTheme([@opaque] IconTheme.t)
  | SetTokenTheme([@opaque] TokenTheme.t)
  | SetColorTheme([@opaque] Theme.t)
  | StatusBarAddItem([@opaque] StatusBarModel.Item.t)
  | StatusBarDisposeItem(int)
  | StatusBar(StatusBarModel.action)
  | ViewCloseEditor(int)
  | ViewSetActiveEditor(int)
  | EnableZenMode
  | DisableZenMode
  | CopyActiveFilepathToClipboard
  | SearchStart
  | SearchHotkey
  | Search(Feature_Search.msg)
  | Sneak(Sneak.action)
  | PaneTabClicked(Pane.paneType)
  | VimDirectoryChanged(string)
  // "Internal" effect action, see TitleStoreConnector
  | SetTitle(string)
  | Noop
and command = {
  commandCategory: option(string),
  commandName: string,
  commandAction: t,
  commandEnabled: unit => bool,
  commandIcon: [@opaque] option(IconTheme.IconDefinition.t),
}
// [configurationTransformer] is a function that modifies configuration json
and configurationTransformer = Yojson.Safe.t => Yojson.Safe.t
and tick = {
  deltaTime: float,
  totalTime: float,
}
and editor = {
  editorId: EditorId.t,
  bufferId: int,
  scrollX: float,
  scrollY: float,
  minimapMaxColumnWidth: int,
  minimapScrollY: float,
  /*
   * The maximum line visible in the view.
   * TODO: This will be dependent on line-wrap settings.
   */
  maxLineLength: int,
  viewLines: int,
  cursors: [@opaque] list(Vim.Cursor.t),
  selection: [@opaque] VisualRange.t,
}
and editorMetrics = {
  pixelWidth: int,
  pixelHeight: int,
  lineHeight: float,
  characterWidth: float,
}
and editorGroup = {
  editorGroupId: int,
  activeEditorId: option(int),
  editors: [@opaque] IntMap.t(editor),
  bufferIdToEditorId: [@opaque] IntMap.t(int),
  reverseTabOrder: list(int),
  metrics: editorMetrics,
}
and menuItem = {
  category: option(string),
  name: string,
  command: unit => t,
  icon: [@opaque] option(IconTheme.IconDefinition.t),
  highlight: list((int, int)),
}
and quickmenuVariant =
  | CommandPalette
  | EditorsPicker
  | FilesPicker
  | Wildmenu([@opaque] Vim.Types.cmdlineType)
  | ThemesPicker
  | DocumentSymbols
and progress =
  | Loading
  | InProgress(float)
  | Complete;
