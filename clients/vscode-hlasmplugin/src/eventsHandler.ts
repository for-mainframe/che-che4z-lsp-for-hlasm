/*
 * Copyright (c) 2019 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

import * as vscode from 'vscode';

import { ConfigurationsHandler } from './configurationsHandler'
import { isLineContinued } from './customEditorCommands';
import { HLASMLanguageDetection } from './hlasmLanguageDetection'

/**
 * Handles various events happening in VSCode
 */
export class EventsHandler {
    private readonly isInstruction: RegExp;
    private readonly isTrigger: RegExp;
    private readonly completeCommand: string;
    // several events update/need configuration information
    private configSetup: ConfigurationsHandler;
    // newly open files are detected for HLASM
    private langDetect: HLASMLanguageDetection;
    // parse in progress indicator
    /**
     * @param completeCommand Used to invoke complete manually in continuationHandling mode
     * @param highlight Shows/hides parsing progress
     */
    constructor(completeCommand: string) {
        this.isInstruction = new RegExp("^([^*][^*]\\S*\\s+\\S+|\\s+\\S*)$");
        this.isTrigger = new RegExp("^[a-zA-Z\*\@\#\$\_]+$");
        this.completeCommand = completeCommand;
        this.configSetup = new ConfigurationsHandler();
        this.langDetect = new HLASMLanguageDetection(this.configSetup);

        this.setWildcards();
    }

    dispose() { }

    // when contents of a document change, issue a completion request
    onDidChangeTextDocument(event: vscode.TextDocumentChangeEvent, continuationOffset: number): boolean {
        if (getConfig<boolean>('continuationHandling', false)) {
            if (event.document.languageId != 'hlasm')
                return false;

            //const editor = vscode.window.activeTextEditor;
            if (event.contentChanges.length == 0 || event.document.languageId != "hlasm")
                return false;

            const change = event.contentChanges[0];
            const currentLine = event.document.getText(
                new vscode.Range(
                    new vscode.Position(
                        change.range.start.line, 0),
                    change.range.start));

            const notContinued = change.range.start.line == 0 ||
                !isLineContinued(event.document, change.range.start.line, continuationOffset);

            if ((currentLine != "" &&
                this.isTrigger.test(change.text) &&
                this.isInstruction.test(currentLine) &&
                notContinued &&
                currentLine[0] != "*") || change.text == "." || change.text == "&") {
                vscode.commands.executeCommand(this.completeCommand);
                return true;
            }
        }
        return false;
    }

    // when document opens, show parse progress
    async onDidOpenTextDocument(document: vscode.TextDocument) {
        await this.editorChanged(document);
    }

    onDidChangeConfiguration(event: vscode.ConfigurationChangeEvent) {
        if (event.affectsConfiguration("hlasm.continuationHandling"))
            vscode.commands.executeCommand("workbench.action.reloadWindow");
    }

    // when active editor changes, try to set a language for it
    async onDidChangeActiveTextEditor(editor?: vscode.TextEditor) {
        if (editor)
            await this.editorChanged(editor.document);
    }

    // when pgm_conf changes, update wildcards
    async onDidSaveTextDocument(document: vscode.TextDocument) {
        const workspace = vscode.workspace.getWorkspaceFolder(document.uri);
        if (workspace) {
            await this.configSetup.generateWildcards(workspace.uri, document.uri).then(wildcards =>
                this.configSetup.updateWildcards(workspace.uri, wildcards ?? [])
            );
        }
    }

    // should the configs be checked
    private async editorChanged(document: vscode.TextDocument) {
        if (document.isClosed)
            return;

        // delay the autodetection as it can apparently trigger an infinite loop of
        // opening and closing of the file
        setTimeout(() => {
            if (!document.isClosed) this.langDetect.setHlasmLanguage(document);
        }, 50);
    }

    async onDidChangeWorkspaceFolders(wsChanges: vscode.WorkspaceFoldersChangeEvent) {
        await this.setWildcards();
    }

    private async setWildcards() {
        this.configSetup.setWildcards(
            (await Promise.all(
                (vscode.workspace.workspaceFolders || []).map(
                    x => this.configSetup.generateWildcards(x.uri).then((regset) => regset?.map(regex => { return { regex, workspaceUri: x.uri }; }) ?? [])
                )
            )
            ).flat()
        );
    }
}

/**
 * Method to get workspace configuration option
 * @param option name of the option (e.g. for hlasmplugin.path should be path)
 * @param defaultValue default value to return if option is not set
 */
export function getConfig<T>(option: string, defaultValue: T) {
    const config = vscode.workspace.getConfiguration('hlasm');
    return config.get<T>(option, defaultValue);
}
