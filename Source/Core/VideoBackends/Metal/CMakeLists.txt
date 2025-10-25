add_library(videometal
  MRCHelpers.h
  MTLBoundingBox.mm
  MTLBoundingBox.h
  MTLGfx.mm
  MTLGfx.h
  MTLMain.mm
  MTLObjectCache.h
  MTLObjectCache.mm
  MTLPerfQuery.mm
  MTLPerfQuery.h
  MTLPipeline.mm
  MTLPipeline.h
  MTLShader.mm
  MTLShader.h
  MTLStateTracker.mm
  MTLStateTracker.h
  MTLTexture.mm
  MTLTexture.h
  MTLUtil.mm
  MTLUtil.h
  MTLVertexFormat.mm
  MTLVertexFormat.h
  MTLVertexManager.mm
  MTLVertexManager.h
  VideoBackend.h
)

find_library(METAL_LIBRARY Metal)
find_library(QUARTZCORE_LIBRARY QuartzCore)

target_link_libraries(videometal
PUBLIC
  common
  videocommon
PRIVATE
  spirv_cross
  ${FOUNDATION_LIBRARY}
  ${METAL_LIBRARY}
  ${QUARTZCORE_LIBRARY}
)

target_compile_options(videometal PRIVATE -fno-objc-arc)
