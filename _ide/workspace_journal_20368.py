# 2026-05-22T10:20:09.405176900
import vitis

client = vitis.create_client()
client.set_workspace(path="MicroBlaze_AC850.sw")

platform = client.create_platform_component(name = "platform_new",hw_design = "$COMPONENT_LOCATION/../../MicroBlaze_MIPS_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

comp = client.create_app_component(name="app_hello_world",platform = "$COMPONENT_LOCATION/../platform_new/export/platform_new/platform_new.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="platform_new")
status = platform.build()

comp = client.get_component(name="app_hello_world")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

status = platform.build()

comp.build()

vitis.dispose()

