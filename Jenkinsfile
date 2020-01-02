pipeline {
  agent any
  stages {
    stage('Prvni') {
      parallel {
        stage('Prvni') {
          steps {
            echo 'Prvni zprava'
          }
        }

        stage('spi 1') {
          steps {
            sleep 1
          }
        }

        stage('spi 2') {
          steps {
            sleep 2
          }
        }

      }
    }

    stage('Druhá') {
      parallel {
        stage('Druhá') {
          steps {
            echo 'Druhá zpráva'
          }
        }

        stage('a cekej') {
          steps {
            sleep 3
          }
        }

      }
    }

    stage('Cekej') {
      steps {
        sleep 3
        echo 'Cekej 3 s'
      }
    }

    stage('Posledni') {
      steps {
        sleep 2
        echo 'Cekej 2s a posledni zprava'
      }
    }

  }
}