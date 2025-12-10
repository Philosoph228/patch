# Writing and Running Tests

Currently the project uses Visual Studio CppUnitTestFramework for testing.

### Key Requirements

* The repository must remain unchanged during:
  * runtine execution
  * build processes
  * test execution 

> [!NOTE]
> The layout of the repository is **immutable at runtime**: running executables, building the project, or executing tests **must not modify the directory structure or file contents**. The only allowed modifications by runtime are performed by **code generation scripts**. These scripts are executed manually or as part of the build pipeline, and their output is commited when appropriate.

## Directory Structure

```
.
├── src/                # Application source code
├── tests/              # Automated tests
│   └── data/           # The sample data to run tests with
├── scripts/            # Code generation scripts (allowed to modify repo)
├── docs/               # Documentation and guidelines
├── project/            # Project files for IDEs
│   └── vs2022/         # Visual Studio 2022 .sln and .vcxproj files
├── build/              # Build outputs (not committed)
└── README.md
```

> [!NOTE]
> temporary files, caches, or build artifacts must not be stored in the repository.
