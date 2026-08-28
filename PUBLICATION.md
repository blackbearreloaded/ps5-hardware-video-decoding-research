# Publication policy

This repository is an independent interoperability research publication. Its
purpose is to document measured behavior and independently authored integration
patterns, not to distribute vendor implementation material or enable access to
copyrighted works.

## Content boundary

The public repository may contain:

- independently written explanations and examples;
- measurements produced on lawfully accessed, authorized equipment;
- standard codec, color, and image-layout terminology;
- public platform API names when needed to explain interoperability; and
- semantic behavior necessary to reproduce a result without vendor code.

Do not commit:

- SDK headers, libraries, firmware, application binaries, or extracted code;
- shader binaries, analysis databases, memory dumps, or proprietary media;
- exact function addresses, private symbol identifiers, opaque command values,
  non-public selector values, or title-specific identifiers;
- signing, encryption, authentication, account, device, or stream secrets; or
- instructions or tooling whose purpose is to defeat an access control.

Prefer the least detail that still explains the independently observed result.
Keep private laboratory records outside this repository.

## Contributor checklist

Before publication, confirm that a change:

1. is original or used under a compatible license with required attribution;
2. contains no third-party binary, implementation extract, secret, or private
   identifier;
3. describes an interoperability result rather than a way to gain unauthorized
   access;
4. separates measured facts from inference and untested claims; and
5. is safe in the current tree and in every Git commit that will be published.

## Legal-review limit

This policy is publication hygiene, not legal advice or legal clearance. Laws,
licenses, contracts, and platform terms differ by jurisdiction and circumstance.
In the United States, [17 U.S.C. § 1201](https://uscode.house.gov/view.xhtml?req=granuleid:USC-prelim-title17-section1201&num=0&edition=prelim)
includes a conditional interoperability provision; it is not blanket permission
and it does not displace other law.
Obtain advice from qualified counsel before relying on an exception or publishing
material whose provenance or authorization is uncertain.
