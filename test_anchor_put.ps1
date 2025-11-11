# Test script for sending anchor configuration to ESP32 SignalK
# This simulates what 6Pack app sends

$ESP32_IP = "192.168.4.1"
$PORT = "3000"

# The anchor configuration data from 6Pack
$anchorData = @{
    anchor = @{
        enabled = $false
        radius = 30
        lat = 59.88183333333333
        lon = 23.346116666666667
        ts = 1762839677604
    }
    depth = @{
        alarm = $false
        min_depth = 2.5
    }
    wind = @{
        alarm = $false
        max_speed = 50
    }
} | ConvertTo-Json -Depth 10

Write-Host "Testing PUT request to ESP32 SignalK server..."
Write-Host "URL: http://${ESP32_IP}:${PORT}/signalk/v1/api/vessels/self/navigation/anchor/akat"
Write-Host ""
Write-Host "Sending data:"
Write-Host $anchorData
Write-Host ""

try {
    $response = Invoke-WebRequest `
        -Uri "http://${ESP32_IP}:${PORT}/signalk/v1/api/vessels/self/navigation/anchor/akat" `
        -Method PUT `
        -Body $anchorData `
        -ContentType "application/json" `
        -UseBasicParsing

    Write-Host "Response Status: $($response.StatusCode)"
    Write-Host "Response Body: $($response.Content)"
} catch {
    Write-Host "Error: $_"
    if ($_.Exception.Response) {
        $reader = [System.IO.StreamReader]::new($_.Exception.Response.GetResponseStream())
        $responseBody = $reader.ReadToEnd()
        Write-Host "Response Body: $responseBody"
    }
}

Write-Host ""
Write-Host "Now test GET to verify data was stored:"
Write-Host "curl http://${ESP32_IP}:${PORT}/signalk/v1/api/vessels/self/navigation/anchor/akat"
