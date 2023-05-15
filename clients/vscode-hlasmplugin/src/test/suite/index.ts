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

import * as path from 'path';
import * as Mocha from 'mocha';
import * as glob from 'glob';
import * as vscode from 'vscode';
import * as process from 'process';
import { popWaitRequestResolver, timeout } from './testHelper';
import { EXTENSION_ID, activate } from '../../extension';
import { ClientUriDetails, ExternalRequestType } from '../../hlasmExternalFiles';

async function primeExtension(): Promise<vscode.Disposable[]> {
	const ext = await vscode.extensions.getExtension<ReturnType<typeof activate>>(EXTENSION_ID)!.activate();
	const lang: {
		sendRequest<R>(method: string, param: any, token?: vscode.CancellationToken): Promise<R>;
	} = ext.getExtension()!;
	// prime opcode suggestions to avoid timeouts
	await Promise.race([lang.sendRequest<object>('textDocument/$/opcode_suggestion', { opcodes: ['OPCODE'] }), timeout(30000, 'Opcode suggestion request failed')]);

	const fileClientMock = {
		getConnInfo: () => Promise.resolve({ info: '', uniqueId: undefined }),
		parseArgs(p: string, _purpose: ExternalRequestType) {
			const [path, file] = p.split('/').slice(1).map(x => x.toUpperCase());
			return {
				path: path || '',
				file: (file || '').split('.')[0],
				toDisplayString() { return `${this.path}/${this.file}`; },
				normalizedPath() { return `/${this.path}/${this.file}`; },
			}
		},
		createClient: () => {
			return {
				connect: (_: string) => Promise.resolve(),
				listMembers: (_: { path: string, file: string } & ClientUriDetails) => {
					return Promise.resolve(['MACA', 'MACB', 'MACC']);
				},
				readMember: (args: { path: string, file: string } & ClientUriDetails) => {
					if (/^MAC[A-C]$/.test(args.file))
						return Promise.resolve(`.*
          MACRO
          ${args.file}
          MEND`);

					return Promise.resolve(null);
				},

				dispose: () => { },

				reusable: () => true,
			};
		},
	};

	ext.registerExternalFileClient('TEST', fileClientMock);

	return [vscode.debug.registerDebugAdapterTrackerFactory('hlasm', {
		createDebugAdapterTracker: function (session: vscode.DebugSession): vscode.ProviderResult<vscode.DebugAdapterTracker> {
			return {
				onDidSendMessage: (message: any) => {
					if (message.type !== 'response')
						return;
					const resolver = popWaitRequestResolver(message.command, session.id);
					if (resolver)
						resolver();
				}
			};
		}
	})];
}

export async function run(): Promise<void> {
	const is_theia = 'THEIA_PARENT_PID' in process.env;

	// Create the mocha test
	const mocha = new Mocha({ ui: 'tdd', color: true });
	const testsPath = path.join(__dirname, '..');

	const files = await new Promise<string[]>((resolve, reject) => {
		glob((!is_theia) ? '**/**.test.js' : '**/integration.test.js', { cwd: testsPath }, (err, files) => {
			if (err)
				reject(err);
			else
				resolve(files);
		});
	});

	// Add files to the test suite
	files.forEach(file => mocha.addFile(path.resolve(testsPath, file)));

	const toDispose = await primeExtension();

	await new Promise((resolve, reject) => {
		// Run the mocha test
		mocha.run(failures => {
			if (failures > 0) {
				if (is_theia)
					console.error('>>>THEIA TESTS FAILED<<<');
				reject(new Error(`${failures} tests failed.`));
			} else {
				resolve(undefined);
			}
		});
	}).finally(() => { toDispose.forEach(d => d.dispose()) });

	if (is_theia)
		console.log('>>>THEIA TESTS PASSED<<<');
}
