# Publishing vectordb_io

The package is published by `.github/workflows/publish-pypi.yml`. It builds and
tests the macOS arm64 and Linux x86_64 wheels, builds and tests the source
distribution, and only then publishes all three distributions together.

## Accounts and permissions

1. Create a [PyPI account](https://pypi.org/account/register/), verify its email,
   and enable two-factor authentication. The first successful publication
   creates `vectordb_io` under the account that configured its pending publisher.
   A PyPI organization is not required.
2. To validate a release on TestPyPI first, create a separate
   [TestPyPI account](https://test.pypi.org/account/register/). It is a separate
   service with separate account and publisher settings.
3. On GitHub, repository administrators can configure the environments. A user
   with write access can run the workflow. The repository/organization must
   allow GitHub Actions and the actions referenced by this workflow.
4. The publishing job uses `id-token: write` to authenticate with PyPI through
   OIDC. No PyPI API token or GitHub personal access token needs to be stored in
   repository secrets for this workflow.

## One-time Trusted Publisher setup

In [GitHub repository environments](https://github.com/vectordb-io/vectordb/settings/environments),
create environments named `pypi` and, if using TestPyPI, `testpypi`.

Open [PyPI account publishing](https://pypi.org/manage/account/publishing/),
choose **Add a new pending publisher**, and select GitHub. Fill in:

| Field | Value |
| --- | --- |
| PyPI project name | `vectordb_io` |
| GitHub owner | `vectordb-io` |
| Repository name | `vectordb` |
| Workflow filename | `publish-pypi.yml` |
| Environment name | `pypi` |

For TestPyPI, repeat in
[TestPyPI account publishing](https://test.pypi.org/manage/account/publishing/)
with the same values except environment name **`testpypi`**. A pending publisher
does not reserve the project name; it becomes a project on its first successful
upload. If the project already exists under your account, configure its trusted
publisher from the project's Publishing settings instead; project Owner
permissions are needed to manage publishers.

## Publishing

The project code is MIT licensed. Distributions include `LICENSE` and the
dependency notices in `THIRD_PARTY_LICENSES.txt`. The package version comes from `[workspace.package].version` in
`Cargo.toml`; published filenames cannot be replaced by a rebuilt file under
the same version.

1. Open [Publish Python package](https://github.com/vectordb-io/vectordb/actions/workflows/publish-pypi.yml).
2. Choose **Run workflow**, select `main`, and choose `testpypi` for the first
   validation run. The workflow defaults to TestPyPI.
3. After a successful run, test a clean installation on both supported platforms:

   ```sh
   python -m pip install --index-url https://test.pypi.org/simple/ --only-binary=:all: --no-deps vectordb_io==0.1.0
   python -c 'from vectordb_io import VectorDB; print(VectorDB)'
   ```

4. Run the same workflow with repository **`pypi`**. After successful publication:

   ```sh
   pip install vectordb_io
   ```

   ```python
   from vectordb_io import VectorDB
   ```

The publish job gets OIDC permissions only after all builds and tests succeed.
Its preflight requires exactly two wheels (macOS 11 arm64 and manylinux2014
x86_64), one source distribution, the expected package name, and a matching
version. The builds use Rust 1.89.0, maturin 1.15.0 and, on Linux, Zig 0.13.0.
Source distributions preserve the entire Git-tracked workspace so the included
Cargo.lock remains valid with locked builds.

## References

- [PyPI: create a project through Trusted Publishing](https://docs.pypi.org/trusted-publishers/creating-a-project-through-oidc/)
- [PyPI: publish using OIDC](https://docs.pypi.org/trusted-publishers/using-a-publisher/)
