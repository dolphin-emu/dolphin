// this is a single line of code, i refuse to copyright it

void main() {
  SetOutput(SampleLocation(
      (floor(GetCoordinates() * GetResolution()) + float2(0.5, 0.5)) *
      GetInvResolution()));
}