<#
Simple helper: convert MT6835 12-bit velocity value (from PWM when PWM_SEL=0x2)
to RPM. Use this to test/convert values produced by firmware or monitor.
Usage:
  .\mt6835_velocity_to_rpm.ps1 -VelocityRaw 0x7FF -Mode pwm -PWMFreq 994 -PPR 4096
#>
param(
    [Parameter(Mandatory=$true)][int]$VelocityRaw,
    [ValidateSet('pwm','counts')][string]$Mode = 'pwm',
    [int]$PWMFreq = 994,
    [int]$PPR = 4096
)

# MT6835 notes:
# - When PWM_SEL=0x2 the PWM frame contains 12-bit velocity data.
# - PWM frame frequency is typically 994Hz or 497Hz.
# - Interpretation depends how firmware exposes the value; this helper assumes
#   VelocityRaw is the 12-bit value (0..4095) representing mechanical velocity
#   units matching the device docs (if PWM contains RPM directly, no conversion needed).

function Convert-PWMVelocityToRPM {
    param($vel12, $pwmFreq)
    # Heuristic conversion used by many sensors: velocity (counts per frame)
    # vel12 / 4095 * (pwmFreq * 60) gives rotations per minute if vel12 encodes
    # fraction of full-scale rotations per PWM frame.
    # This is a best-effort formula; adjust if your firmware defines the scale differently.

    $fraction = [double]$vel12 / 4095.0
    $rpm = $fraction * $pwmFreq * 60.0
    return [math]::Round($rpm,2)
}

function Convert-CountsToRPM {
    param($countsPerRev, $ppr)
    # If you have counts per second: multiply by 60 and divide by pulses-per-rev
    return [math]::Round(($countsPerRev * 60.0) / $ppr,2)
}

if ($Mode -eq 'pwm') {
    $rpm = Convert-PWMVelocityToRPM -vel12 $VelocityRaw -pwmFreq $PWMFreq
    $hex = '{0:X}' -f $VelocityRaw
    Write-Output "Mode: PWM -> VelocityRaw=$VelocityRaw (0x$hex) => RPM=$rpm (using PWMFreq=$PWMFreq Hz)"
} else {
    # treat VelocityRaw as counts/sec
    $rpm = Convert-CountsToRPM -countsPerRev $VelocityRaw -ppr $PPR
    Write-Output "Mode: counts -> CountsPerSec=$VelocityRaw => RPM=$rpm (PPR=$PPR)"
}
