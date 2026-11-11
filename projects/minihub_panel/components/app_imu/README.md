# app_imu

Publishes the Mosaico BMI270 as MiniHub Point `mosaico.imu` (`type=sensor`,
`no_control`). Shake, tilt and flip become `value` plus `props.params.motion`
/`gesture` so rules can trigger on `when.point_id=mosaico.imu`.

The sampler task owns the BMI270 for process lifetime. Init failure is
non-fatal to the panel. Host simulation does not compile this component.
