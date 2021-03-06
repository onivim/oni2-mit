/*
 * ExtensionClientStoreConnector.re
 *
 * This connects the extension client to the store:
 * - Converts extension host notifications into ACTIONS
 * - Calls appropriate APIs on extension host based on ACTIONS
 */

open EditorCoreTypes;
open Oni_Core;
open Oni_Model;

module Uri = Oni_Core.Uri;
module Log = (val Log.withNamespace("Oni2.Extension.ClientStore"));
module Option = Utility.Option;

open Oni_Extensions;
module Extensions = Oni_Extensions;
module Protocol = Extensions.ExtHostProtocol;

module Workspace = Protocol.Workspace;

module ExtensionCompletionProvider = {
  let suggestionItemToCompletionItem:
    Protocol.SuggestionItem.t => CompletionItem.t =
    suggestion => {
      let completionKind =
        suggestion.kind |> Option.bind(CompletionItemKind.ofInt);

      {
        label: suggestion.label,
        kind: completionKind,
        detail: suggestion.detail,
      };
    };

  let suggestionsToCompletionItems:
    option(Protocol.Suggestions.t) => list(CompletionItem.t) =
    fun
    | Some(suggestions) =>
      List.map(suggestionItemToCompletionItem, suggestions)
    | None => [];

  let create =
      (
        client: ExtHostClient.t,
        {id, selector}: Protocol.SuggestProvider.t,
        (buffer, _completionMeet, location),
      ) =>
    ProviderUtility.runIfSelectorPasses(
      ~buffer,
      ~selector,
      () => {
        let uri = Buffer.getUri(buffer);
        let position = Protocol.OneBasedPosition.ofPosition(location);
        ExtHostClient.provideCompletions(id, uri, position, client)
        |> Lwt.map(suggestionsToCompletionItems);
      },
    );
};

module ExtensionDefinitionProvider = {
  let definitionToModel = def => {
    let Protocol.DefinitionLink.{uri, range, originSelectionRange} = def;
    let Range.{start, _} = Protocol.OneBasedRange.toRange(range);

    let originRange =
      originSelectionRange |> Option.map(Protocol.OneBasedRange.toRange);

    LanguageFeatures.DefinitionResult.create(
      ~originRange,
      ~uri,
      ~location=start,
    );
  };

  let create =
      (client, {id, selector}: Protocol.BasicProvider.t, (buffer, location)) => {
    ProviderUtility.runIfSelectorPasses(
      ~buffer,
      ~selector,
      () => {
        let uri = Buffer.getUri(buffer);
        let position = Protocol.OneBasedPosition.ofPosition(location);
        ExtHostClient.provideDefinition(id, uri, position, client)
        |> Lwt.map(definitionToModel);
      },
    );
  };
};

module ExtensionDocumentHighlightProvider = {
  let definitionToModel = (highlights: list(Protocol.DocumentHighlight.t)) => {
    highlights
    |> List.map(highlights =>
         Protocol.OneBasedRange.toRange(
           Protocol.DocumentHighlight.(highlights.range),
         )
       );
  };

  let create =
      (client, {id, selector}: Protocol.BasicProvider.t, (buffer, location)) => {
    ProviderUtility.runIfSelectorPasses(
      ~buffer,
      ~selector,
      () => {
        let uri = Buffer.getUri(buffer);
        let position = Protocol.OneBasedPosition.ofPosition(location);

        ExtHostClient.provideDocumentHighlights(id, uri, position, client)
        |> Lwt.map(definitionToModel);
      },
    );
  };
};

module ExtensionFindAllReferencesProvider = {
  let create =
      (client, {id, selector}: Protocol.BasicProvider.t, (buffer, location)) => {
    ProviderUtility.runIfSelectorPasses(
      ~buffer,
      ~selector,
      () => {
        let uri = Buffer.getUri(buffer);
        let position = Protocol.OneBasedPosition.ofPosition(location);

        ExtHostClient.provideReferences(id, uri, position, client);
      },
    );
  };
};

module ExtensionDocumentSymbolProvider = {
  let create =
      (client, {id, selector, _}: Protocol.DocumentSymbolProvider.t, buffer) => {
    ProviderUtility.runIfSelectorPasses(
      ~buffer,
      ~selector,
      () => {
        let uri = Buffer.getUri(buffer);
        ExtHostClient.provideDocumentSymbols(id, uri, client);
      },
    );
  };
};

let start = (extensions, setup: Setup.t) => {
  let (stream, dispatch) = Isolinear.Stream.create();

  let manifests =
    List.map((ext: ExtensionScanner.t) => ext.manifest, extensions);

  let defaults = Configuration.Model.ofExtensions(manifests);
  let keys = ["reason_language_server.location"];

  let contents =
    `Assoc([
      (
        "reason_language_server",
        `Assoc([("location", `String(setup.rlsPath))]),
      ),
    ]);
  let user = Configuration.Model.create(~keys, contents);

  let initialConfiguration = Configuration.create(~defaults, ~user, ());

  let onExtHostClosed = () => Log.debug("ext host closed");

  let extensionInfo =
    extensions
    |> List.map(ext =>
         Extensions.ExtHostInitData.ExtensionInfo.ofScannedExtension(ext)
       );

  let onDiagnosticsClear = owner => {
    dispatch(Actions.DiagnosticsClear(owner));
  };

  let onDiagnosticsChangeMany =
      (diagCollection: Protocol.DiagnosticsCollection.t) => {
    let protocolDiagToDiag: Protocol.Diagnostic.t => Diagnostic.t =
      d => {
        let range = Protocol.OneBasedRange.toRange(d.range);
        let message = d.message;
        Diagnostic.create(~range, ~message, ());
      };

    let f = (d: Protocol.Diagnostics.t) => {
      let diagnostics = List.map(protocolDiagToDiag, snd(d));
      let uri = fst(d);
      Actions.DiagnosticsSet(uri, diagCollection.name, diagnostics);
    };

    diagCollection.perFileDiagnostics
    |> List.map(f)
    |> List.iter(a => dispatch(a));
  };

  let onStatusBarSetEntry = ((id, text, alignment, priority)) => {
    dispatch(
      Actions.StatusBarAddItem(
        StatusBarModel.Item.create(
          ~id,
          ~text,
          ~alignment=StatusBarModel.Alignment.ofInt(alignment),
          ~priority,
          (),
        ),
      ),
    );
  };

  let onRegisterDefinitionProvider = (client, provider) => {
    let id =
      Protocol.BasicProvider.("exthost." ++ string_of_int(provider.id));
    let definitionProvider =
      ExtensionDefinitionProvider.create(client, provider);

    dispatch(
      Actions.LanguageFeature(
        LanguageFeatures.DefinitionProviderAvailable(id, definitionProvider),
      ),
    );
  };

  let onRegisterDocumentSymbolProvider = (client, provider) => {
    let id =
      Protocol.DocumentSymbolProvider.(
        "exthost." ++ string_of_int(provider.id)
      );
    let documentSymbolProvider =
      ExtensionDocumentSymbolProvider.create(client, provider);

    dispatch(
      Actions.LanguageFeature(
        LanguageFeatures.DocumentSymbolProviderAvailable(
          id,
          documentSymbolProvider,
        ),
      ),
    );
  };

  let onRegisterReferencesProvider = (client, provider) => {
    let id =
      Protocol.BasicProvider.("exthost." ++ string_of_int(provider.id));
    let findAllReferencesProvider =
      ExtensionFindAllReferencesProvider.create(client, provider);

    dispatch(
      Actions.LanguageFeature(
        LanguageFeatures.FindAllReferencesProviderAvailable(
          id,
          findAllReferencesProvider,
        ),
      ),
    );
  };

  let onRegisterDocumentHighlightProvider = (client, provider) => {
    let id =
      Protocol.BasicProvider.("exthost." ++ string_of_int(provider.id));
    let documentHighlightProvider =
      ExtensionDocumentHighlightProvider.create(client, provider);

    dispatch(
      Actions.LanguageFeature(
        LanguageFeatures.DocumentHighlightProviderAvailable(
          id,
          documentHighlightProvider,
        ),
      ),
    );
  };

  let onRegisterSuggestProvider = (client, provider) => {
    let id =
      Protocol.SuggestProvider.("exthost." ++ string_of_int(provider.id));
    let completionProvider =
      ExtensionCompletionProvider.create(client, provider);

    dispatch(
      Actions.LanguageFeature(
        LanguageFeatures.CompletionProviderAvailable(id, completionProvider),
      ),
    );
  };

  let onOutput = Log.info;

  let onDidActivateExtension = id => {
    dispatch(Actions.Extension(Oni_Model.Extensions.Activated(id)));
  };

  let onShowMessage = message => {
    dispatch(Actions.ShowNotification(Notification.create(message)));
  };

  let initData = ExtHostInitData.create(~extensions=extensionInfo, ());
  let extHostClient =
    Extensions.ExtHostClient.start(
      ~initialConfiguration,
      ~initialWorkspace=Workspace.fromPath(Sys.getcwd()),
      ~initData,
      ~onClosed=onExtHostClosed,
      ~onStatusBarSetEntry,
      ~onDiagnosticsClear,
      ~onDiagnosticsChangeMany,
      ~onDidActivateExtension,
      ~onRegisterDefinitionProvider,
      ~onRegisterDocumentHighlightProvider,
      ~onRegisterDocumentSymbolProvider,
      ~onRegisterReferencesProvider,
      ~onRegisterSuggestProvider,
      ~onShowMessage,
      ~onOutput,
      setup,
    );

  let _bufferMetadataToModelAddedDelta =
      (bm: Vim.BufferMetadata.t, fileType: option(string)) =>
    switch (bm.filePath, fileType) {
    | (Some(fp), Some(ft)) =>
      Log.debug("Creating model for filetype: " ++ ft);
      Some(
        Protocol.ModelAddedDelta.create(
          ~uri=Uri.fromPath(fp),
          ~versionId=bm.version,
          ~lines=[""],
          ~modeId=ft,
          ~isDirty=true,
          (),
        ),
      );
    | _ => None
    };

  let activatedFileTypes: Hashtbl.t(string, bool) = Hashtbl.create(16);

  let activateFileType = (fileType: option(string)) =>
    fileType
    |> Option.iter(ft =>
         Hashtbl.find_opt(activatedFileTypes, ft)
         // If no entry, we haven't activated yet
         |> Option.iter_none(() => {
              ExtHostClient.activateByEvent(
                "onLanguage:" ++ ft,
                extHostClient,
              );
              Hashtbl.add(activatedFileTypes, ft, true);
            })
       );

  let sendBufferEnterEffect =
      (bm: Vim.BufferMetadata.t, fileType: option(string)) =>
    Isolinear.Effect.create(~name="exthost.bufferEnter", () =>
      switch (_bufferMetadataToModelAddedDelta(bm, fileType)) {
      | None => ()
      | Some((v: Protocol.ModelAddedDelta.t)) =>
        activateFileType(fileType);
        ExtHostClient.addDocument(v, extHostClient);
      }
    );

  let modelChangedEffect = (buffers: Buffers.t, bu: BufferUpdate.t) =>
    Isolinear.Effect.create(~name="exthost.bufferUpdate", () =>
      switch (Buffers.getBuffer(bu.id, buffers)) {
      | None => ()
      | Some(v) =>
        Oni_Core.Log.perf("exthost.bufferUpdate", () => {
          let modelContentChange =
            Protocol.ModelContentChange.ofBufferUpdate(
              bu,
              Protocol.Eol.default,
            );
          let modelChangedEvent =
            Protocol.ModelChangedEvent.create(
              ~changes=[modelContentChange],
              ~eol=Protocol.Eol.default,
              ~versionId=bu.version,
              (),
            );

          let uri = Buffer.getUri(v);

          ExtHostClient.updateDocument(
            uri,
            modelChangedEvent,
            true,
            extHostClient,
          );
        })
      }
    );

  let executeContributedCommandEffect = cmd =>
    Isolinear.Effect.create(~name="exthost.executeContributedCommand", () => {
      ExtHostClient.executeContributedCommand(cmd, extHostClient)
    });

  let discoveredExtensionsEffect = extensions =>
    Isolinear.Effect.createWithDispatch(
      ~name="exthost.discoverExtensions", dispatch =>
      dispatch(
        Actions.Extension(Oni_Model.Extensions.Discovered(extensions)),
      )
    );

  let registerQuitCleanupEffect =
    Isolinear.Effect.createWithDispatch(
      ~name="exthost.registerQuitCleanup", dispatch =>
      dispatch(
        Actions.RegisterQuitCleanup(
          () => ExtHostClient.close(extHostClient),
        ),
      )
    );

  let changeWorkspaceEffect = path =>
    Isolinear.Effect.create(~name="exthost.changeWorkspace", () => {
      ExtHostClient.acceptWorkspaceData(
        Workspace.fromPath(path),
        extHostClient,
      )
    });

  let updater = (state: State.t, action) =>
    switch (action) {
    | Actions.Init => (
        state,
        Isolinear.Effect.batch([
          registerQuitCleanupEffect,
          discoveredExtensionsEffect(extensions),
        ]),
      )
    | Actions.BufferUpdate(bu) => (
        state,
        modelChangedEffect(state.buffers, bu),
      )
    | Actions.CommandExecuteContributed(cmd) => (
        state,
        executeContributedCommandEffect(cmd),
      )
    | Actions.VimDirectoryChanged(path) => (
        state,
        changeWorkspaceEffect(path),
      )
    | Actions.BufferEnter(bm, fileTypeOpt) => (
        state,
        sendBufferEnterEffect(bm, fileTypeOpt),
      )
    | _ => (state, Isolinear.Effect.none)
    };

  (updater, stream);
};
