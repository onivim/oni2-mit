open EditorCoreTypes;
open Revery;
open Revery.UI;
open Revery.UI.Components;
open Oni_Core;

module Option = Utility.Option;
module Path = Utility.Path;
module Log = (val Log.withNamespace("Oni2.UI.LocationListView"));

type item = {
  file: string,
  location: Location.t,
  text: string,
  highlight: option((Index.t, Index.t)),
};

// TODO: move to Revery
let getFontAdvance = (fontFile, fontSize) => {
  open Revery.Draw;

  let maybeWindow = Revery.UI.getActiveWindow();
  let scaledFontSize =
    Text._getScaledFontSizeFromWindow(maybeWindow, fontSize);
  let font = FontCache.load(fontFile, scaledFontSize);
  let shapedText = FontRenderer.shape(font, "x");
  let Fontkit.{advance, _} =
    FontRenderer.getGlyph(font, shapedText[0].glyphId);

  let multiplier =
    switch (maybeWindow) {
    | None => 1.0
    | Some(w) => Window.getScaleAndZoom(w) *. Window.getDevicePixelRatio(w)
    };
  float(advance) /. (64. *. multiplier);
};

module Styles = {
  open Style;

  let clickable = [cursor(Revery.MouseCursors.pointer)];

  let result = (~theme: Theme.t, ~isHovered) => [
    flexDirection(`Row),
    overflow(`Hidden),
    paddingVertical(4),
    paddingHorizontal(8),
    backgroundColor(
      isHovered ? theme.menuSelectionBackground : Colors.transparentWhite,
    ),
  ];

  let locationText = (~font: UiFont.t, ~theme: Theme.t) => [
    fontFamily(font.fontFile),
    fontSize(font.fontSize),
    color(theme.editorActiveLineNumberForeground),
    textWrap(TextWrapping.NoWrap),
  ];

  let snippet = (~font: EditorFont.t, ~theme: Theme.t, ~isHighlighted) => [
    fontFamily(font.fontFile),
    fontSize(font.fontSize),
    color(
      isHighlighted ? theme.oniNormalModeBackground : theme.editorForeground,
    ),
    textWrap(TextWrapping.NoWrap),
  ];
};

let item =
    (
      ~theme,
      ~uiFont: UiFont.t,
      ~editorFont,
      ~onMouseOver,
      ~onMouseOut,
      ~width,
      ~isHovered,
      ~item,
      ~onSelect,
      (),
    ) => {
  let workingDirectory = Rench.Environment.getWorkingDirectory(); // TODO: This should be workspace-relative

  let onClick = () => onSelect(item);

  let locationText =
    Printf.sprintf(
      "%s:%n - ",
      Path.toRelative(~base=workingDirectory, item.file),
      Index.toOneBased(item.location.line),
    );

  let locationWidth = {
    let window = Revery.UI.getActiveWindow();
    Revery.Draw.Text.measure(
      ~window,
      ~fontSize=uiFont.fontSize,
      ~fontFamily=uiFont.fontFile,
      locationText,
    ).
      width;
  };

  let location = () =>
    <Text
      style={Styles.locationText(~font=uiFont, ~theme)}
      text=locationText
    />;

  let content = () => {
    let unstyled = (~text, ()) =>
      <Text
        style={Styles.snippet(~font=editorFont, ~theme, ~isHighlighted=false)}
        text
      />;

    let highlighted = (~text, ()) =>
      <Text
        style={Styles.snippet(~font=editorFont, ~theme, ~isHighlighted=true)}
        text
      />;

    switch (item.highlight) {
    | Some((indexStart, indexEnd)) =>
      open Utility.StringUtil;

      let availableWidth = float(width - locationWidth);
      let maxLength =
        int_of_float(availableWidth /. editorFont.measuredWidth);
      let charStart = Index.toZeroBased(indexStart);
      let charEnd = Index.toZeroBased(indexEnd);

      try({
        let (text, charStart, charEnd) =
          extractSnippet(~maxLength, ~charStart, ~charEnd, item.text);
        let before = String.sub(text, 0, charStart);
        let matchedText = String.sub(text, charStart, charEnd - charStart);
        let after = String.sub(text, charEnd, String.length(text) - charEnd);

        <View style=Style.[flexDirection(`Row)]>
          <unstyled text=before />
          <highlighted text=matchedText />
          <unstyled text={trimRight(after)} />
        </View>;
      }) {
      | Invalid_argument(message) =>
        // TODO: This shouldn't happen, but you never know. Consider a sane implementation of `String.sub` instead, to avoid this
        Log.errorf(m =>
          m("\"%s\" - (%n, %n)\n%!", message, charStart, charEnd)
        );
        <unstyled text={item.text} />;
      };
    | None => <unstyled text={item.text} />
    };
  };

  <Clickable style=Styles.clickable onClick>
    <View style={Styles.result(~theme, ~isHovered)} onMouseOver onMouseOut>
      <location />
      <content />
    </View>
  </Clickable>;
};

let%component make =
              (
                ~theme: Theme.t,
                ~uiFont: UiFont.t,
                ~editorFont: EditorFont.t,
                ~items: array(item),
                ~onSelectItem: item => unit,
                (),
              ) => {
  let%hook (outerRef, setOuterRef) = Hooks.ref(None);
  let%hook (hovered, setHovered) = Hooks.state(-1);

  let editorFont = {
    ...editorFont,
    fontSize: uiFont.fontSize,
    measuredWidth: getFontAdvance(editorFont.fontFile, uiFont.fontSize),
    // measuredHeight:
    //   editorFont.measuredHeight
    //   *. (float(uiFont.fontSize) /. float(editorFont.fontSize)),
  };

  let width =
    outerRef
    |> Option.map(node => node#measurements().Dimensions.width)
    |> Option.value(
         ~default=
           Revery.UI.getActiveWindow()
           |> Option.map((window: Window.t) => window.metrics.size.width)
           |> Option.value(~default=4000),
       );

  let renderItem = i => {
    let onMouseOver = _ => setHovered(_ => i);
    let onMouseOut = _ => setHovered(hovered => i == hovered ? (-1) : hovered);

    <item
      theme
      uiFont
      editorFont
      onMouseOver
      onMouseOut
      width
      isHovered={hovered == i}
      item={Array.get(items, i)}
      onSelect=onSelectItem
    />;
  };

  <FlatList
    rowHeight=20
    count={Array.length(items)}
    focused=None
    ref={ref => setOuterRef(Some(ref))}>
    ...renderItem
  </FlatList>;
};
