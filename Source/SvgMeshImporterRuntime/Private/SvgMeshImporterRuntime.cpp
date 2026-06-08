#include "SvgMeshImporterRuntime.h"
#include "SvgMeshImporterLog.h"

#define LOCTEXT_NAMESPACE "FSvgMeshImporterRuntimeModule"

DEFINE_LOG_CATEGORY(LogSvgMeshImporter);

void FSvgMeshImporterRuntimeModule::StartupModule()
{
}

void FSvgMeshImporterRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSvgMeshImporterRuntimeModule, SvgMeshImporterRuntime)
