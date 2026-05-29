# 2026-05-29T08:01:46.991049600
import vitis

client = vitis.create_client()
client.set_workspace(path="MicroBlaze_AC850.sw")

comp = client.get_component(name="app_hello_world")
status = comp.clean()

platform = client.get_component(name="platform_new")
status = platform.build()

comp.build()

comp = client.create_app_component(name="app_parallel_IO",platform = "$COMPONENT_LOCATION/../platform_new/export/platform_new/platform_new.xpfm",domain = "standalone_microblaze_0")

status = platform.build()

comp = client.get_component(name="app_parallel_IO")
comp.build()

vitis.dispose()

