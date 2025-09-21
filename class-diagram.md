# AK App Class Diagram

```mermaid
classDiagram
    class Config {
        +configDir: string
        +vaultPath: string
        +profilesDir: string
        +gpgAvailable: bool
        +json: bool
        +forcePlain: bool
        +presetPassphrase: string
        +auditLogPath: string
        +instanceId: string
        +persistDir: string
    }

    class KeyStore {
        +kv: map~string,string~
        +getValue(keyName: string): string
        +setValue(keyName: string, value: string)
        +removeKey(keyName: string)
        +hasKey(keyName: string): bool
    }

    class Profile {
        +name: string
        +keys: map~string,string~
        +keyCount: int
        +isDefault: bool
        +loadProfile(config: Config, name: string)
        +saveProfile(config: Config)
        +addKey(keyName: string, keyValue: string)
        +removeKey(keyName: string)
        +getKey(keyName: string): string
        +getKeys(): map~string,string~
        +hasKey(keyName: string): bool
        +validateKeyUniqueness(service: string, keyName: string): bool
    }

    class ProfileManager {
        +config: Config
        +profiles: vector~Profile~
        +currentProfile: string
        +defaultProfile: string
        +loadProfiles()
        +createProfile(name: string): Profile
        +deleteProfile(name: string)
        +renameProfile(oldName: string, newName: string)
        +duplicateProfile(sourceName: string, targetName: string)
        +importProfile(filePath: string)
        +exportProfile(name: string, filePath: string)
        +validateProfileName(name: string): bool
        +getDefaultProfile(): Profile
        +setDefaultProfile(name: string)
        +ensureDefaultProfile()
    }

    class Service {
        +name: string
        +keyName: string
        +description: string
        +testEndpoint: string
        +testMethod: string
        +testHeaders: string
        +authMethod: string
        +testable: bool
        +isBuiltIn: bool
        +validateKey(key: string): bool
        +getDefaultKeyName(): string
        +test(apiKey: string): TestResult
        +updateConfiguration(endpoint: string, method: string, headers: string)
        +getServiceInfo(): map~string,string~
    }

    class ServiceManager {
        +config: Config
        +services: map~string,Service~
        +loadServices()
        +saveServices()
        +addService(service: Service)
        +removeService(name: string)
        +updateService(service: Service)
        +getServiceByName(name: string): Service
        +detectConfiguredServices(profileName: string): vector~string~
        +getAllServiceKeys(): map~string,string~
        +getBuiltInServices(): map~string,Service~
    }

    class ApiKey {
        +name: string
        +value: string
        +serviceName: string
        +serviceUrl: string
        +masked: bool
        +profileName: string
        +isDefaultProfile: bool
        +validateName(): bool
        +maskValue(): string
        +detectService(): string
    }

    class KeyManager {
        +config: Config
        +currentProfile: string
        +keys: map~string,ApiKey~
        +refreshKeys(profileName: string)
        +addKey(key: ApiKey, profileName: string)
        +editKey(name: string, newValue: string, profileName: string)
        +deleteKey(name: string, profileName: string)
        +searchKeys(filter: string, profileName: string): vector~ApiKey~
        +toggleVisibility(keyName: string)
        +validateKeyName(name: string): bool
        +enforceServiceKeyLimit(profile: Profile, service: Service, keyName: string): bool
        +loadKeysForProfile(profileName: string)
        +saveKeysForProfile(profileName: string)
    }

    class TestResult {
        +service: string
        +ok: bool
        +duration: chrono_milliseconds
        +errorMessage: string
        +keyName: string
    }

    class ServiceTester {
        +config: Config
        +testApiKey(service: Service, apiKey: string): TestResult
        +testAllKeys(profile: Profile): vector~TestResult~
        +runTestsParallel(services: vector~string~, failFast: bool): vector~TestResult~
        +validateConnection(endpoint: string, headers: string): bool
    }

    class ProfileVault {
        +config: Config
        +profileName: string
        +loadProfileVault(profileName: string): map~string,string~
        +saveProfileVault(profileName: string, keys: map~string,string~)
        +encryptData(data: string): string
        +decryptData(encryptedData: string): string
        +createBackup(profileName: string)
        +migrateFromGlobalVault()
    }

    class KeyConstraint {
        +serviceName: string
        +keyPattern: string
        +maxKeysPerProfile: int
        +allowDuplicateNames: bool
        +validateKeyName(keyName: string, profile: Profile): bool
        +checkServiceLimit(profile: Profile, service: Service): bool
    }

    %% Relationships (classDiagram style)
    Config "1" --> "many" Profile : config for
    Config "1" --> "1" KeyStore
    Config "1" --> "1" Vault

    ProfileManager "1" --> "many" Profile : manages
    ProfileManager "1" --> "1" Config

    Profile "1" --> "many" ApiKey
    Profile "1" --> "1" KeyConstraint

    KeyManager "1" --> "1" ProfileVault
    KeyManager "1" --> "many" ApiKey
    KeyManager "1" --> "1" Config

    ServiceManager "1" o-- "many" Service
    ServiceManager "1" --> "1" Config


    ApiKey "many" --> "1" Service
    ApiKey "many" --> "1" KeyConstraint

    ServiceTester "1" --> "many" TestResult
    ServiceTester "1" --> "many" Service
    ServiceTester "1" --> "many" ApiKey

    ProfileVault "1" --> "many" Profile
    ProfileVault "1" --> "1" Config

    KeyConstraint "many" --> "1" Service
```

## Key Design Decisions

### 1. **Profile-Centric Architecture**
- Profiles serve as containers for collections of API keys
- Each profile represents a specific environment or use case
- Profiles enforce the "one service API key per profile" constraint

### 2. **Service Abstraction**
- Abstract Service class allows for both built-in and custom services
- BuiltinService for predefined services (OpenAI, Anthropic, etc.)
- CustomService for user-defined services with configurable endpoints

### 3. **Key Constraint System**
- KeyConstraint class enforces business rules
- Prevents duplicate service keys within a profile unless different names are used
- Validates key naming patterns and service limits

### 4. **Flexible Key Naming**
- Users can have multiple keys for the same service using different names
- Example: `OPENAI_API_KEY`, `OPENAI_API_KEY_2`, `OPENAI_API_KEY_PROD`
- Maintains service association while allowing flexibility

### 5. **Testing Integration**
- ServiceTester provides API key validation
- TestResult captures test outcomes
- Supports both individual and batch testing

### 6. **Secure Storage**
- Vault handles encrypted storage via GPG
- KeyStore provides key-value storage abstraction
- Supports both encrypted and plain text storage modes