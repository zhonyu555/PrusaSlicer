pipeline {
  agent any
  stages {
    stage('Build') {
      steps {
        sh 'docker build . -t marek9336/PrusaSlicer'
      }
    }

  }
}