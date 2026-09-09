# Contributing to Opener

## Signing off on your commits

*Opener* uses the [Developer Certificate of Origin (DCO)](https://developercertificate.org/) rather than a Contributor License Agreement (CLA). This means that you are required to sign off on every commit to signal that you affirm the DCO, which essentially states that you have the right to contribute your work and that you understand the contribution and its sign-off become a permanent part of the public Git history. Since you are personally certifying your contribution, sign off with a name you consistently go by and an email address at which you can be reached. Fully anonymous sign-offs are not accepted.

The same mechanism is used by upstream Zephyr, so if you have contributed there, the workflow will already be familiar. The simplest way to add a `Signed-off-by` line to your commits is to let Git handle it for you:

```
git commit -s
```

This appends a line in the form:

```
Signed-off-by: Jane Doe <jane@example.com>
```

Git fills in the name and email from your `user.name` and `user.email` settings.

## Coding and style guidelines

*Opener* adopts the coding and style conventions from Zephyr, rather than defining its own:

- [Coding Guidelines](https://docs.zephyrproject.org/latest/contribute/coding_guidelines/index.html)
- [Style Guidelines](https://docs.zephyrproject.org/latest/contribute/style/index.html)

The `.clang-format` at the repository root is copied from upstream Zephyr; running `clang-format` handles most formatting for you. Because clang-format's output can change between releases, please use version 22.1.5, which is the version pinned in CI.

## What we expect in a pull request

These guidelines are a starting point and will evolve over time. When in doubt, open an issue and ask.

- **One self-contained logical change per pull request.** Small, clearly scoped PRs keep each change easy to reason about and avoid the situation where one part of a PR is ready to merge while another still needs major revision. If you are planning a larger feature, please open an issue to discuss the approach before investing significant work.
- **Commit messages that explain the change.** Avoid huge commits and break your changes into smaller pieces instead. Use a concise title in the imperative form ("Add ...", "Fix ..."), followed by a body that explains what the change does and why. Every commit needs a `Signed-off-by` line, as described [above](#signing-off-on-your-commits).
- **Traceability to the specification.** For non-trivial protocol logic, reference the relevant sections of ETSI TS 103 636 in the commit message or pull request description.
- **A note on how you verified the change.** Please describe in the pull request how you convinced yourself that the change works. If sensible, add a test.
- **Green CI.** The CI checks (formatting, DCO sign-off) must pass before a pull request can be merged.

## Use of AI assistants in contributions

Large Language Models (LLMs) and AI-based coding tools have become part of many developers' everyday workflows. The *Opener Initiative* does not prohibit their use, but we ask contributors to apply them thoughtfully and within the expectations described below.

The following three considerations shape our policy:

1. **Legal integrity:** *Opener* is explicitly intended for integration into commercial products and released under Apache 2.0, which requires contributors to have the right to license their contributions. AI coding assistants can, in rare cases, reproduce material from training data under incompatible licenses. Moreover, there's ongoing legal ambiguity about the copyright status of purely AI-generated output. It's important to keep the codebase cleanly licensable.
2. **Technical correctness:** LLMs likely have had limited exposure to ETSI TS 103 636 during training and tend to produce output that sounds plausible but misinterprets the specification in subtle ways. Our goal of passing ETSI-defined conformance tests leaves no room for such errors to slip through.
3. **Accountability:** Reviewers and adopters of the codebase need to trust that every contribution has been understood and vouched for by a human contributor who stands behind it.

Following from these considerations, our policy is built around the principle that human contributors are fully responsible for everything they submit, regardless of how it was produced. By opening a pull request you confirm that:

- You understand the code you are submitting and can explain and defend its design during review.
- You have the right to contribute it under Apache 2.0, and you have taken reasonable steps to ensure it does not incorporate material from sources with incompatible licenses.
- You have reviewed the output of any AI tool you used, rather than pasting it unchanged.
- If a substantial portion of the contribution was drafted with AI assistance, a brief note in the pull request description is appreciated.

For non-trivial protocol logic, we expect clear references to the relevant sections of ETSI TS 103 636, adequate tests, and a rationale the contributor can stand behind. This is the most effective safeguard against specification misinterpretations, whether introduced by humans or by AI.

Major design decisions are made through open discussion in issues or community meetings, with the rationale documented. AI tools may support individual contributors in thinking through options, but the decisions themselves are made through human deliberation within the community.

To summarize: Be transparent, take responsibility for your contribution, and hold AI-assisted work to the same standard as anything else you would submit. If you are unsure whether a particular use of AI assistance fits within these expectations, open an issue or raise it in the relevant discussion channel before starting significant work — we would rather clarify early than rework late.
