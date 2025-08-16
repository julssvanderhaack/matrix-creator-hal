import sys
sys.path.insert(0, "/home/alejandro/julss/matrix-creator-hal/build/tfg")
import matrix_hal_tfg as tfg
tfg.run(16000, 5, 3, "beamformed_output.wav")
